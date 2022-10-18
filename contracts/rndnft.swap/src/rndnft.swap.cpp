#include <amax.ntoken/amax.ntoken.hpp>
#include "rndnft.swap.hpp"
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


void rndnft_swap::init( const name& admin, const name& fund_distributor){

    //CHECKC( false, err::MISC ,"error");
    require_auth( _self );
    _gstate.admin  = admin;
//    booth_t::idx_t booth( get_self(), get_self().value);
//    auto p_itr = booth.begin();
//    while( p_itr != booth.end() ){
//        p_itr = booth.erase( p_itr );
//    }
}

void rndnft_swap::createbooth( const name& owner, const booth_conf_s& conf ) {
    CHECKC( has_auth(get_self()) || has_auth(_gstate.admin), err::NO_AUTH, "Missing required authority of admin or maintainer" );
    
    CHECKC( is_account(owner), err::ACCOUNT_INVALID,                    "owner doesnot exist" )
    CHECKC( is_account(conf.base_nft_contract), err::ACCOUNT_INVALID,   "base_nft_contract doesnot exist" )
    CHECKC( is_account(conf.quote_nft_contract), err::ACCOUNT_INVALID,  "quote_nft_contract doesnot exist" )
    CHECKC( conf.quote_nft_price.amount > 0, err::PARAM_ERROR ,         "price amount not positive" )

    auto booth                   = booth_t( ++_gstate.last_booth_id );
    booth.conf                   = conf;
    booth.created_at             = current_time_point();

    _db.set( booth );

}

void rndnft_swap::enablebooth(const name& owner, const uint64_t& booth_id, bool enabled) {
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


void rndnft_swap::setboothtime( const name& owner, const uint64_t& booth_id, const time_point_sec& opened_at, 
                            const time_point_sec& closed_at ) {
    CHECKC( has_auth( owner ) || has_auth( _self ) || has_auth(_gstate.admin), err::NO_AUTH, "not authorized" )

    auto booth = booth_t( booth_id );
    CHECKC( _db.get( booth ),  err::RECORD_NOT_FOUND, "booth not found: " + to_string(booth_id) )
    CHECKC( owner == booth.owner,  err::NO_AUTH, "non-booth-owner unauthorized" )

    booth.conf.opened_at         = opened_at;
    booth.conf.closed_at         = closed_at;
    booth.updated_at             = current_time_point();

    _db.set( booth );
}


/// @brief - user to send blindbox nft tokens into the booth to swap
/// @param from 
/// @param to 
/// @param assets 
/// @param memo -  
///             format1: refuel:$booth_id
///             format2: swap:$booth_id    
void rndnft_swap::on_transfer_ntoken( const name& from, const name& to, const vector<nasset>& assets, const string& memo) {
    if (from == get_self() || to != get_self()) return;
    require_auth( from );

    vector<string_view> memo_params = split(memo, ":");
    ASSERT( memo_params.size() > 0 )
    CHECKC( memo_params[0] == "refuel" || memo_params[0] == "swap", err::MEMO_FORMAT_ERROR, "memo must start with 'refuel' or 'swap'" )
    CHECKC (memo_params.size() == 2, err::MEMO_FORMAT_ERROR, "params size not equal to 2" )

    auto booth_id            = std::stoul(string(memo_params[1]));
    auto booth               = booth_t( booth_id );
    CHECKC( _db.get( booth ), err::RECORD_NOT_FOUND, "booth not found: " + to_string(booth_id) )
    CHECKC( booth.status == booth_status::enabled, err::STATUS_ERROR, "booth not enabled, status:" + booth.status.to_string() )

    if( memo_params[0] == "refuel" ) {
        _refule_nft( booth, assets );

    } else { //swap
        CHECKC( assets.size() == 1, err::OVERSIZED, "must be 1 asset only to swap in" )
        _swap_nft( from, booth, assets[0] );
    }
}

void rndnft_swap::_refule_nft( booth_t& booth, const vector<nasset>& assets ) {
    uint64_t new_nft_num        = 0;
    uint64_t new_nftbox_num     = 0;
    for( nasset nft : assets ) {
        CHECKC( nft.amount > 0, err::NOT_POSITIVE, "NFT amount must be positive" )

        auto nftboxes = booth_nftbox_t::idx_t( _self, booth.id );
        auto nftidx = nftboxes.get_index<"nftidx"_n>();
        auto itr = nftidx.lower_bound( nft.symbol.id );

        if( itr == nftidx.end() ) {
            auto id = nftboxes.available_primary_key();
            auto nftbox = booth_nftbox_t(id);
            nftbox.nfts = nft;

            _db.set( booth.id, nftbox );
            new_nftbox_num++;

        } else {
            auto id = itr->id;
            auto nftbox = booth_nftbox_t(id);
            _db.get( nftbox );

            nftbox.nfts += nft;
        }

      
        new_nft_num             += nft.amount;

        _db.set(nftbox);
    }

    booth.base_nft_sum          += new_nft_num;
    booth.base_nft_available    += new_nft_num;
    booth.base_nftbox_available += new_nftbox_num;
    booth.updated_at            = current_time_point();

    _db.set( booth.id, booth );
}

void rndnft_swap::_swap_nft( const name& user, const booth_t& booth, const nasset& paid_nft ) {
    CHECKC( booth.conf.quote_nft_contract == get_first_receiver(), err::DATA_MISMATCH, "sent NFT mismatches with booth's quote nft" );
    CHECKC( booth.base_nft_available > 0, err::OVERSIZED, "zero nft left" )
    
    // auto swap_amount = asset.amount / booth.quote_nft_price.amount;
    // CHECKC( swap_amount <= _gstate.batch_swap_max_nfts, err::OVERSIZED, "oversized swap_amount: " + to_string(swap_amount) )

    // booth.quote_nft_recd        += paid_nft;
    // auto now                    = time_point_sec( current_time_point() );
    // for( size_t i; i < swap_amount; i++ ) {
    //     nasset nft;
    //     _one_nft( now, user, booth, nft );
    //     vector<nasset> nfts = { nft };
    //     TRANSFER_N( booth.conf.base_nft_contract, user, nfts, "swap@" + to_string(booth.id) )

    //     auto trace = deal_trace_s_s( );
    //     trace.booth_id          = booth_id;
    //     trace.user              = user;
    //     trace.base_nft_contract = booth.conf.base_nft_contract;
    //     trace.quote_nft_contract= booth.conf.quote_nft_contract;
    //     trace.paid_quant        = paid_nft;
    //     trace.sold_quant        = nft;
    //     trace.created_at        = now;
    //     _on_deal_trace_s(trace);
    // }
}

void rndnft_swap::closebooth(const name& owner, const uint64_t& booth_id){
    CHECKC( has_auth( owner ) || has_auth( _self ) || has_auth(_gstate.admin), err::NO_AUTH, "not authorized to end booth" )
    
    auto booth = booth_t( booth_id );
    CHECKC( _db.get( booth ), err::RECORD_NOT_FOUND, "booth not found: " + to_string(booth_id) )
    CHECKC( booth.status == booth_status::enabled, err::STATUS_ERROR, "booth not enabled, status:" + booth.status.to_string() )
    CHECKC( booth.conf.owner == owner, err::NO_AUTH, "not authorized to end booth" );

    _db.del( booth );

}

void rndnft_swap::dealtrace(const deal_trace_s_s& trace) {
    require_auth(get_self());
    require_recipient(trace.user);
}

void rndnft_swap::_one_nft( const time_point_sec& now, const name& owner, booth_t& booth, nasset& nft ) {
    auto boothboxes = booth_nftbox_t( booth.id );
    CHECKC( _db.get( booth.id, boothboxes ), err::RECORD_NOT_FOUND, "no nftbox in the booth" )

    uint64_t rand                       = _rand( 1, booth.base_nft_available, booth.owner, booth.id );
    uint64_t curr_num                   = 0;

    auto boxes_idx = booth_nftbox_t::idx_t(_self, booth.id);
    auto itr = boxes_idx.begin();
    advance( itr, rand - 1 );

    if( itr != boxes_idx.end() ) {
        booth.base_nft_available--;
        nft = itr->nfts;
        nft.amount = 1;

        // itr->nfts -= nft;
        boxes_idx.modify(*itr, same_payer, [&]( auto& row ) {
            row.nfts -= nft;
        });
        
        if( itr->nfts.amount == 0) {
            booth.base_nftbox_available--;
            boxes_idx.erase( itr );
        }
    }

    booth.updated_at  = now;
    _db.set( booth );

}

uint64_t rndnft_swap::_rand(const uint16_t& min, const uint64_t& max, const name& owner, const uint64_t& booth_id) {
    auto mixid = tapos_block_prefix() * tapos_block_num() + owner.value + booth_id - current_time_point().sec_since_epoch();
    const char *mixedChar = reinterpret_cast<const char *>( &mixid );
    auto hash = sha256( (char *)mixedChar, sizeof(mixedChar));

    auto r1 = (uint64_t) hash.data()[0];
    uint64_t rand = r1 % max + min;
    return rand;
}

void rndnft_swap::_on_deal_trace_s(const deal_trace_s_s& deal_trace_s) {
    rndnft_swap::deal_trace_s_action act{ _self, { {_self, active_permission} } };
    act.send( deal_trace_s );
}
