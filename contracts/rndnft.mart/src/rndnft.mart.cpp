#include <amax.ntoken/amax.ntoken.hpp>
#include "rndnft.mart.hpp"
#include "commons/utils.hpp"
#include "commons/wasm_db.hpp"
#include "commons/math.hpp"
#include <aplink.farm/aplink.farm.hpp>

#include <cstdlib>
#include <ctime>

#include <chrono>

using std::chrono::system_clock;
using namespace wasm;
using namespace amax;
using namespace std;

// static constexpr eosio::name active_permission{"active"_n};
static constexpr symbol   APL_SYMBOL          = symbol(symbol_code("APL"), 4);

#define TRANSFER(bank, to, quantity, memo) \
{ action(permission_level{get_self(), "active"_n }, bank, "transfer"_n, std::make_tuple( _self, to, quantity, memo )).send(); }

#define ALLOT_APPLE(farm_contract, lease_id, to, quantity, memo) \
    {   aplink::farm::allot_action(farm_contract, { {_self, active_perm} }).send( \
            lease_id, to, quantity, memo );}

void rndnft_mart::createbooth( const name& owner,const string& title, const name& nft_contract, const name& fund_contract,
                           const uint64_t& split_plan_id, const asset& price, const time_point_sec& opened_at, const uint64_t& duration_days){
 
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
    booth.closed_at             = opened_at + duration_days * DAY_SECONDS;

    CHECKC( booth.closed_at > now, err::PARAM_ERROR,          "close_at must be > opened_at")


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
    auto now                = time_point_sec(current_time_point());
    CHECKC( opened_at < closed_at && closed_at > now, err::PARAM_ERROR, "close_at must be > opened_at")

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
                nfts += nft.amount;
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
    CHECKC( booth.nft_box_num <= _gstate.max_booth_boxes, err::OVERSIZED, "booth box num exceeds allowed" )
        
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
    auto booth_id           = std::stoul(string(memo_params[1]));
    auto booth              = booth_t( booth_id );
    CHECKC( _db.get( booth ), err::RECORD_NOT_FOUND, "booth not found: " + to_string(booth_id) )
    CHECKC( booth.status    == booth_status::enabled, err::STATUS_ERROR, "booth not enabled, status:" + booth.status.to_string() )
    CHECKC( booth.opened_at <= now, err::STATUS_ERROR, "booth not open yet" )
    CHECKC( booth.closed_at >= now,err::STATUS_ERROR, "booth closed already" )
    CHECKC( booth.fund_contract == get_first_receiver(), err::DATA_MISMATCH, "pay contract mismatches with specified" );
    CHECKC( booth.price.symbol == quantity.symbol, err::SYMBOL_MISMATCH, "pay symbol mismatches with price" );
    CHECKC( booth.nft_num > 0, err::OVERSIZED, "zero nft left" )

    auto count              = quantity.amount / booth.price.amount;
    CHECKC( count > 0, err::PARAM_ERROR, "min purchase amount must be >= 1");
    CHECKC( count <= booth.nft_num, err::PARAM_ERROR, "Insufficient remaining quantity");
    // CHECKC( count <= _gstate.max_nfts_purchase , err::PARAM_ERROR, "max purchase amount must be <= " + to_string( _gstate.max_nfts_purchase ));

    auto paid               = booth.price * count;
    auto left               = quantity - paid;
    booth.fund_recd         += paid;
    booth.updated_at        = now;
    auto recv_memo          = "plan:" + to_string( booth.split_plan_id ) + ":1";
    TRANSFER( booth.fund_contract, _gstate.fund_distributor, paid, recv_memo )
    if (left.amount > 0) {
        TRANSFER( booth.fund_contract, from, left, std::string("rndnft.mart change") )
    }

    map<uint64_t, nasset> bought;
    for (int i = 0; i < count; i++) {
        nasset nft;
        _one_nft( from, i, booth, nft );
        if (bought.count( nft.symbol.id ) == 0)
            bought[nft.symbol.id] = nft;
        else
            bought[nft.symbol.id] += nft;
    }

    vector<nasset> nfts = {};
    for (auto const& nft : bought) {
        nfts.emplace_back( nft.second );
        
        auto trace              = deal_trace_s_s( );
        trace.booth_id          = booth_id;
        trace.buyer             = from;
        trace.nft_contract      = booth.nft_contract;
        trace.fund_contract     = booth.fund_contract;
        trace.paid_quant        = booth.price * nft.second.amount;
        trace.sold_quant        = nft.second;
        trace.created_at        = now;
        _on_deal_trace_s(trace);
    }
    TRANSFER_N( booth.nft_contract, from, nfts , "booth: " + to_string(booth.id) )
    
    _reward_farmer( paid, from );
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
        refund_nfts.emplace_back( nasset(itr->second,itr->first) );
    }
    _db.del( boothboxes );
    _db.del( booth );

    if ( refund_nfts.size() > 0 )
        TRANSFER_N( booth.nft_contract, owner, refund_nfts, std::string("closebooth") )

}

void rndnft_mart::dealtrace(const deal_trace_s_s& trace) {
    require_auth(get_self());
    require_recipient(trace.buyer);
}

void rndnft_mart::_reward_farmer( const asset& quantity, const name& farmer ) {
   if ( _gstate1.apl_farm.lease_id == 0 ) return;
    
   auto apples = asset(0, APLINK_SYMBOL);
   aplink::farm::available_apples( _gstate1.apl_farm.contract, _gstate1.apl_farm.lease_id, apples );
   if (apples.amount == 0) return;

   auto symbol_code = quantity.symbol.code().to_string();
   if (_gstate1.apl_farm.reward_conf.find(symbol_code) == _gstate1.apl_farm.reward_conf.end())
      return;

   auto unit_reward_quant = _gstate1.apl_farm.reward_conf[symbol_code];
   if (unit_reward_quant.amount == 0)
      return;

   auto reward_amount = safemath::mul( unit_reward_quant.amount, quantity.amount, get_precision(quantity.symbol) );
   auto reward_quant = asset( reward_amount, APL_SYMBOL );
   ALLOT_APPLE( _gstate1.apl_farm.contract, _gstate1.apl_farm.lease_id, farmer, reward_quant, "rndnft.mart APL reward" )
}


void rndnft_mart::_one_nft( const name& owner, const uint64_t& index, booth_t& booth, nasset& nft ) {
    auto boothboxes = booth_nftbox_t( booth.id );
    CHECKC( _db.get( boothboxes ), err::RECORD_NOT_FOUND, "no nftbox in the booth" )
  
    uint64_t rand_index = _rand( boothboxes.nfts.size(), owner, index );
    auto itr = boothboxes.nfts.begin();
    if (rand_index > 0)
        std::advance(itr, rand_index );

    nft = nasset( 1, itr->first );
    itr->second         -= 1;
    booth.nft_num       -= 1;

    if (itr->second == 0) {
        boothboxes.nfts.erase( itr );
        booth.nft_box_num--;
    }

    _db.set( boothboxes );
    _db.set( booth );

}

uint64_t rndnft_mart::_rand(const uint64_t& range, const name& owner, const uint64_t& index) {
    auto rnd_factors    = to_string(tapos_block_prefix() * tapos_block_num()) + owner.to_string() + to_string(index * 100);
    auto hash           = HASH256( rnd_factors );
    auto r1             = (uint64_t) (hash.data()[7] << 56) | 
                          (uint64_t) (hash.data()[6] << 48) | 
                          (uint64_t) (hash.data()[5] << 40) |
                          (uint64_t) (hash.data()[4] << 32) | 
                          (uint64_t) (hash.data()[3] << 24) |
                          (uint64_t) (hash.data()[2] << 16) |
                          (uint64_t) (hash.data()[1] << 8)  |
                          (uint64_t) hash.data()[0];

    uint64_t rand       = r1 % range;

    return rand;
}

void rndnft_mart::_on_deal_trace_s(const deal_trace_s_s& deal_trace_s) {
    rndnft_mart::deal_trace_s_action act{ _self, { {_self, active_permission} } };
    act.send( deal_trace_s );
}
