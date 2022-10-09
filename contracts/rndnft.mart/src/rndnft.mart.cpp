#include <amax.ntoken/amax.ntoken.hpp>
#include "rndnft.mart.hpp"
#include "commons/utils.hpp"
#include "commons/wasm_db.hpp"

#include <cstdlib>
#include <ctime>

#include <chrono>

using std::chrono::system_clock;
using namespace wasm;
using namespace amax;
using namespace std;

#define TRANSFER(bank, to, quantity, memo) \
{ action(permission_level{get_self(), "active"_n }, bank, "transfer"_n, std::make_tuple( _self, to, quantity, memo )).send(); }


void rndnft_mart::init( const name& admin, const name& fund_distributor){

    //CHECKC( false, err::MISC ,"error");
    require_auth( _self );
    _gstate.admin  = admin;
    _gstate.fund_distributor = fund_distributor;
//    booth_t::tbl_t booth( get_self(), get_self().value);
//    auto p_itr = booth.begin();
//    while( p_itr != booth.end() ){
//        p_itr = booth.erase( p_itr );
//    }
}

void rndnft_mart::createbooth( const name& owner,const string& title, const name& nft_contract, const name& fund_contract,
                           const uint64_t& split_plan_id, const asset& price, const time_point_sec& opened_at, const uint64_t& opened_days){
 
    CHECKC( has_auth(get_self()) || has_auth(_gstate.admin), err::NO_AUTH, "Missing required authority of admin or maintainer" );
    
    CHECKC( is_account(owner),          err::ACCOUNT_INVALID,   "owner does not exist" )
    CHECKC( is_account(fund_contract),  err::ACCOUNT_INVALID,   "fund contract does not exist" )
    CHECKC( is_account(nft_contract),   err::ACCOUNT_INVALID,   "nft contract does not exist" )
    CHECKC( price.amount > 0,           err::PARAM_ERROR ,      "price amount not positive" )

    auto now                    = current_time_point();
    auto booth                  = booth_t( ++_gstate.last_booth_id );

    booth.owner                 = owner;
    booth.title                 = title;
    booth.nft_contract          = nft_contract;
    booth.fund_contract         = fund_contract;
    booth.split_plan_id         = split_plan_id;
    booth.price                 = price;
    booth.fund_recd             = asset(0, price.symbol);
    booth.status                = booth_status::enabled;
    booth.created_at            = now;
    booth.updated_at            = now;
    booth.opened_at             = opened_at;
    booth.closed_at             = opened_at + opened_days * DAY_SECONDS;

    _db.set( booth );

}

void rndnft_mart::enablebooth(const name& owner, const uint64_t& booth_id, bool enabled) {
    CHECKC( has_auth( owner ) || has_auth( _self ) || has_auth(_gstate.admin), err::NO_AUTH, "not authorized" )

    auto booth = booth_t( booth_id );
    CHECKC( _db.get( booth ),  err::RECORD_NOT_FOUND, "booth not found: " + to_string(booth_id) )
    CHECKC( owner == booth.owner,  err::NO_AUTH, "non-booth-owner unauthorized" )

    auto new_status = enabled ? booth_status::enabled : booth_status::disabled;
    CHECKC( booth.status != new_status, err::STATUS_ERROR, "booth status not changed" )

    booth.status         = new_status;
    booth.updated_at     = current_time_point();

    _db.set( booth );
}


void rndnft_mart::setboothtime( const name& owner, const uint64_t& booth_id, const time_point_sec& opened_at, 
                            const time_point_sec& closed_at ) {
    CHECKC( has_auth( owner ) || has_auth( _self ) || has_auth(_gstate.admin), err::NO_AUTH, "not authorized" )

    auto booth = booth_t( booth_id );
    CHECKC( _db.get( booth ),  err::RECORD_NOT_FOUND, "booth not found: " + to_string(booth_id) )
    CHECKC( owner == booth.owner,  err::NO_AUTH, "non-booth-owner unauthorized" )

    booth.opened_at      = opened_at;
    booth.closed_at      = closed_at;
    booth.updated_at     = current_time_point();

    _db.set( booth );
}


/// @brief admin/owner to add nft tokens into the booth
/// @param from 
/// @param to 
/// @param assets 
/// @param memo:  booth:$booth_id
void rndnft_mart::on_transfer_ntoken( const name& from, const name& to, const vector<nasset>& assets, const string& memo) {
    if (from == get_self() || to != get_self()) return;
    require_auth( from );

    vector<string_view> memo_params = split(memo, ":");
    ASSERT( memo_params.size() > 0 )

    auto now = time_point_sec(current_time_point());
    CHECKC( memo_params[0] == "booth", err::MEMO_FORMAT_ERROR, "memo must start with 'booth'" )
    CHECKC (memo_params.size() == 2, err::MEMO_FORMAT_ERROR, "ontransfer: params size not equal to 2" )

    auto booth_id            = std::stoul(string(memo_params[1]));
    auto booth               = booth_t( booth_id );
    CHECKC( _db.get( booth ), err::RECORD_NOT_FOUND, "booth not found: " + to_string(booth_id) )
    CHECKC( booth.status == booth_status::enabled, err::STATUS_ERROR, "booth not enabled, status:" + booth.status.to_string() )
    CHECKC( booth.nft_contract == get_first_receiver(), err::DATA_MISMATCH, "sent NFT contract mismatches with booth nft" );
    CHECKC( booth.owner == from,  err::NO_AUTH, "non-booth owner not authorized" );

    uint64_t new_nft_num        = 0;
    uint64_t new_nftbox_num     = 0;
    for( nasset nft : assets ) {
        CHECKC( nft.amount > 0, err::NOT_POSITIVE, "NFT amount must be positive" )

        auto nftbox = booth_nftbox_t( booth_id );
        if( _db.get( nftbox )) {
            if( nftbox.nfts.find( nft.symbol ) != nftbox.nfts.end() ) {
                auto nfts = nftbox.nfts[ nft.symbol ];
                nfts = nft.amount;
                nftbox.nfts[ nft.symbol ] = nfts;

            } else {
                nftbox.nfts[ nft.symbol ] = nft.amount;
                new_nftbox_num++;
            }
        } else {
            nftbox.nfts[ nft.symbol ] = nft.amount;
            new_nftbox_num++;
        }
        
        new_nft_num         += nft.amount;

        _db.set(nftbox);
    }

    booth.nft_box_num        += new_nftbox_num;
    CHECKC( booth.nft_box_num < _gstate.max_booth_boxes, err::OVERSIZED, "booth box num exceeds allowed" )
        
    booth.nft_num            += new_nft_num;
    booth.nft_num_sum        += new_nft_num;
    booth.updated_at         = now;

    _db.set( booth );
}


/// buyer to send mtoken to buy NFTs (with random NFT return, 1 only)
/// memo format:     booth:${booth_id}
///
void rndnft_mart::on_transfer_mtoken( const name& from, const name& to, const asset& quantity, const string& memo ) {
    if (from == get_self() || to != get_self()) return;
    
    vector<string_view> memo_params = split(memo, ":");
    CHECKC( memo_params.size() == 2, err::MEMO_FORMAT_ERROR, "ontransfer:issue params size of must be 2")
    CHECKC( memo_params[0] == "booth", err::MEMO_FORMAT_ERROR, "memo must be 'booth'" )

    auto now                = time_point_sec(current_time_point());
    auto booth_id            = std::stoul(string(memo_params[1]));
    auto booth               = booth_t( booth_id );
    CHECKC( _db.get( booth ), err::RECORD_NOT_FOUND, "booth not found: " + to_string(booth_id) )
    CHECKC( booth.status == booth_status::enabled, err::STATUS_ERROR, "booth not enabled, status:" + booth.status.to_string() )
    CHECKC( booth.opened_at <= now, err::STATUS_ERROR, "booth not opened yet" )
    CHECKC( booth.closed_at >= now,err::STATUS_ERROR, "booth closed already" )

    auto count              = quantity.amount / booth.price.amount;
    CHECKC( count == 1, err::PARAM_ERROR, "can only buy just about 1 NFT");
    CHECKC( booth.fund_contract == get_first_receiver(), err::DATA_MISMATCH, "pay contract mismatches with specified" );
    CHECKC( booth.price.symbol == quantity.symbol, err::SYMBOL_MISMATCH, "pay symbol mismatches with price" );
    CHECKC( booth.nft_num > 0, err::OVERSIZED, "zero nft left" )

    booth.fund_recd          += quantity;
    booth.updated_at         = now;
    auto recv_memo           = "plan:" + to_string( booth.split_plan_id );
    TRANSFER( booth.fund_contract, _gstate.fund_distributor, quantity, recv_memo )
    
    nasset nft;
    _one_nft( from, booth, nft );
    vector<nasset> nfts = { nft };
    TRANSFER_N( booth.nft_contract, from, nfts , "booth: " + to_string(booth.id) )
   
    //auto trace = deal_trace_s( booth_id, from, booth.nft_contract, booth.fund_contract, quantity, nft, now );
    auto trace = deal_trace_s( );
    trace.booth_id          = booth_id;
    trace.buyer             = from;
    trace.nft_contract      = booth.nft_contract;
    trace.fund_contract     = booth.fund_contract;
    trace.paid_quant        = quantity;
    trace.sold_quant        = nft;
    trace.created_at        = now;
    _on_deal_trace(trace);
    
}

void rndnft_mart::closebooth(const name& owner, const uint64_t& booth_id){
    CHECKC( has_auth( owner ) || has_auth( _self ) || has_auth(_gstate.admin), err::NO_AUTH, "not authorized to end booth" )
    
    auto booth = booth_t( booth_id );
    CHECKC( _db.get( booth ), err::RECORD_NOT_FOUND, "booth not found: " + to_string(booth_id) )
    CHECKC( booth.status == booth_status::enabled, err::STATUS_ERROR, "booth not enabled, status:" + booth.status.to_string() )
    CHECKC( booth.owner == owner, err::NO_AUTH, "not authorized to end booth" );

    auto boothboxes = booth_nftbox_t( booth_id );
    CHECKC( _db.get( boothboxes ), err::RECORD_NOT_FOUND, "closed" )

    vector<nasset> refund_nfts;
    for( auto itr = boothboxes.nfts.begin(); itr != boothboxes.nfts.end(); itr++ ) { 
        refund_nfts.emplace_back( itr->second );
    }
    _db.del( boothboxes );
    _db.del( booth );

    if ( refund_nfts.size() > 0 )
        TRANSFER_N( booth.nft_contract, owner, refund_nfts, std::string("closebooth") )

}

void rndnft_mart::dealtrace(const deal_trace_s& trace) {
    require_auth(get_self());
    require_recipient(trace.buyer);
}

void rndnft_mart::_one_nft( const name& owner, booth_t& booth, nasset& nft ) {
    auto boothboxes = booth_nftbox_t( booth.id );
    CHECKC( _db.get( boothboxes ), err::RECORD_NOT_FOUND, "no nftbox in the booth" )

    uint64_t rand       = _rand( 1, booth.nft_num, booth.owner, booth.id );
    uint64_t curr_num   = 0;
    for (auto itr = boothboxes.nfts.begin(); itr != boothboxes.nfts.end(); itr++) {
        curr_num        += itr->second;
        if ( rand <= curr_num ) {
            nft = nasset( 1, itr->first );
            itr->second         -= 1;
            booth.nft_num       -= 1;

            if (itr->second == 0) {
                boothboxes.nfts.erase( itr );
                booth.nft_box_num--;
            }
            _db.set( boothboxes );
            break;
        }
    }

    _db.set( booth );

}

uint64_t rndnft_mart::_rand(const uint16_t& min_unit, const uint64_t& max_uint, const name& owner, const uint64_t& booth_id) {
    auto mixid = tapos_block_prefix() * tapos_block_num() + owner.value + booth_id - current_time_point().sec_since_epoch();
    const char *mixedChar = reinterpret_cast<const char *>( &mixid );
    auto hash = sha256( (char *)mixedChar, sizeof(mixedChar));

    auto r1 = (uint64_t) hash.data()[0];
    uint64_t rand = r1 % max_uint + min_unit;
    return rand;
}

void rndnft_mart::_on_deal_trace(const deal_trace_s& deal_trace) {
    rndnft_mart::deal_trace_action act{ _self, { {_self, active_permission} } };
    act.send( deal_trace );
}