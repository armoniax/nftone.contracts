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


void rndnft_mart::init( const name& owner){

    //CHECKC( false, err::MISC ,"error");
    require_auth( _self );
    _gstate.admin  = owner;
    // shop_t::tbl_t shop( get_self(), get_self().value);
    // auto p_itr = shop.begin();
    // while( p_itr != shop.end() ){
    //     p_itr = shop.erase( p_itr );
    // } 
}

void rndnft_mart::createshop( const name& owner,const string& title,const name& fund_contract, const name& nft_contract,
                           const asset& price, const name& fund_receiver, const name& random_type,
                           const time_point_sec& opened_at, const uint64_t& opened_days){
 
    CHECKC( has_auth(get_self()) || has_auth(_gstate.admin), err::NO_AUTH, "Missing required authority of admin or maintainer" );
    
    CHECKC( is_account(owner),          err::ACCOUNT_INVALID,   "owner does not exist" )
    CHECKC( is_account(fund_contract), err::ACCOUNT_INVALID,   "asset contract does not exist" )
    CHECKC( is_account(fund_receiver),   err::ACCOUNT_INVALID,   "fee receiver does not exist" )
    CHECKC( is_account(nft_contract),   err::ACCOUNT_INVALID,   "blinbox contract does not exist" )
    CHECKC( price.amount > 0,           err::PARAM_ERROR ,      "price amount not positive" )

    auto now                    = current_time_point();
    auto shop                   = shop_t( ++_gstate.last_shop_id );

    shop.owner                  = owner;
    shop.title                  = title;
    shop.price                  = price;
    shop.fund_receiver          = fund_receiver;
    shop.fund_contract          = fund_contract;
    shop.nft_contract           = nft_contract;
    shop.random_type            = random_type;
    shop.created_at             = now;
    shop.updated_at             = now;
    shop.opened_at              = opened_at;
    shop.closed_at              = opened_at + opened_days * DAY_SECONDS;
    shop.status                 = shop_status::enabled;

    _db.set( shop );

}

void rndnft_mart::enableshop(const name& owner, const uint64_t& shop_id, bool enabled) {
    CHECKC( has_auth( owner ) || has_auth( _self ) || has_auth(_gstate.admin), err::NO_AUTH, "not authorized" )

    auto shop = shop_t( shop_id );
    CHECKC( _db.get( shop ),  err::RECORD_NOT_FOUND, "shop not found: " + to_string(shop_id) )
    CHECKC( owner == shop.owner,  err::NO_AUTH, "non-shop-owner unauthorized" )

    auto new_status = enabled ? shop_status::enabled : shop_status::disabled;
    CHECKC( shop.status != new_status, err::STATUS_ERROR, "shop status not changed" )

    shop.status         = new_status;
    shop.updated_at     = current_time_point();

    _db.set( shop );
}


void rndnft_mart::setshoptime( const name& owner, const uint64_t& shop_id, const time_point_sec& opened_at, 
                            const time_point_sec& closed_at ) {
    CHECKC( has_auth( owner ) || has_auth( _self ) || has_auth(_gstate.admin), err::NO_AUTH, "not authorized" )

    auto shop = shop_t( shop_id );
    CHECKC( _db.get( shop ),  err::RECORD_NOT_FOUND, "shop not found: " + to_string(shop_id) )
    CHECKC( owner == shop.owner,  err::NO_AUTH, "non-shop-owner unauthorized" )

    shop.opened_at      = opened_at;
    shop.closed_at      = closed_at;
    shop.updated_at     = current_time_point();

    _db.set( shop );
}


/// buyer to send mtoken to buy NFTs (with random NFT return, 1 only)
/// memo format:     shop:${shop_id}
///
void rndnft_mart::on_transfer_mtoken( const name& from, const name& to, const asset& quantity, const string& memo ) {
    if (from == get_self() || to != get_self()) return;
    
    vector<string_view> memo_params = split(memo, ":");
    CHECKC( memo_params.size() == 2, err::MEMO_FORMAT_ERROR, "ontransfer:issue params size of must be 2")
    CHECKC( memo_params[0] == "shop", err::MEMO_FORMAT_ERROR, "memo must be 'shop'" )

    auto now                = time_point_sec(current_time_point());
    auto shop_id            = std::stoul(string(memo_params[1]));
    auto shop               = shop_t( shop_id );
    CHECKC( _db.get( shop ), err::RECORD_NOT_FOUND, "shop not found: " + to_string(shop_id) )
    CHECKC( shop.status == shop_status::enabled, err::STATUS_ERROR, "shop not enabled, status:" + shop.status.to_string() )
    CHECKC( shop.opened_at <= now, err::STATUS_ERROR, "shop not opened yet" )
    CHECKC( shop.closed_at >= now,err::STATUS_ERROR, "shop closed already" )

    auto count              = quantity.amount / shop.price.amount;
    CHECKC( count == 1, err::PARAM_ERROR, "can only buy just about 1 NFT");
    CHECKC( shop.fund_contract == get_first_receiver(), err::DATA_MISMATCH, "pay contract mismatches with specified" );
    CHECKC( shop.price.symbol == quantity.symbol, err::SYMBOL_MISMATCH, "pay symbol mismatches with price" );
    CHECKC( shop.nft_current_amount > 0, err::OVERSIZED, "zero nft left" )

    shop.fund_recd          += quantity;
    shop.updated_at         = now;
    TRANSFER( shop.fund_contract, shop.fund_receiver, quantity, "" )
    
    nasset nft;
    _one_nft( from, shop, nft );
    vector<nasset> quants_to_send = { nft };
    TRANSFER_N( shop.nft_contract, shop.owner, quants_to_send , "" )

    auto trace = deal_trace_s( shop_id, from, shop.nft_contract, shop.fund_contract, quantity, nft, now );
    _on_deal_trace(trace);
   
}

/// @brief admin to add nft tokens into the shop
/// @param from 
/// @param to 
/// @param assets 
/// @param memo:  shop:$shop_id
void rndnft_mart::on_transfer_ntoken( const name& from, const name& to, const vector<nasset>& assets, const string& memo) {
    if (from == get_self() || to != get_self()) return;
    require_auth( from );

    vector<string_view> memo_params = split(memo, ":");
    ASSERT( memo_params.size() > 0 )

    auto now = time_point_sec(current_time_point());
    CHECKC( memo_params[0] == "shop", err::MEMO_FORMAT_ERROR, "memo must start with 'shop'" )
    CHECKC (memo_params.size() == 2, err::MEMO_FORMAT_ERROR, "ontransfer: params size not equal to 2" )

    auto shop_id            = std::stoul(string(memo_params[1]));
    auto shop               = shop_t( shop_id );
    CHECKC( _db.get( shop ), err::RECORD_NOT_FOUND, "shop not found: " + to_string(shop_id) )
    CHECKC( shop.status == shop_status::enabled, err::STATUS_ERROR, "shop not enabled, status:" + shop.status.to_string() )
    CHECKC( shop.nft_contract == get_first_receiver(), err::DATA_MISMATCH, "admin sent NFT mismatches with shop nft" );
    CHECKC( shop.owner == from,  err::NO_AUTH, "non-shop owner not authorized" );

    uint64_t nft_count      = 0;
   
    set<nsymbol> unique_nfts;
    for( nasset nft : assets ) {
        CHECKC( nft.amount > 0, err::NOT_POSITIVE, "NFT amount must be positive" )

        unique_nfts.insert( nft.symbol );

        auto nftbox = nft_box_t((uint64_t) nft.symbol.id);
        if( _db.get( shop_id, nftbox )) {
            nftbox.nft      += nft;
            
        } else {
            nftbox.nft      = nft;
        }

        nft_count           += nft.amount;
    }

    shop.nft_box_num        += unique_nfts.size();
    CHECKC( shop.nft_box_num < _gstate.max_shop_boxes, err::OVERSIZED, "shop box num exceeds allowed" )

    if( shop.random_type == nft_random_type::bynftids )
        shop.random_base     = shop.nft_box_num;

    else if( shop.random_type == nft_random_type::bynftamt )
        shop.random_base     += nft_count;
        
    
    shop.nft_current_amount  += nft_count;
    shop.updated_at          = now;

    _db.set( shop );
}

void rndnft_mart::closeshop(const name& owner, const uint64_t& shop_id){
    CHECKC( has_auth( owner ) || has_auth( _self ) || has_auth(_gstate.admin), err::NO_AUTH, "not authorized to end shop" )
    
    auto shop = shop_t( shop_id );
    CHECKC( _db.get( shop ), err::RECORD_NOT_FOUND, "shop not found: " + to_string(shop_id) )
    CHECKC( shop.status == shop_status::enabled, err::STATUS_ERROR, "shop not enabled, status:" + shop.status.to_string() )
    CHECKC( shop.owner == owner, err::NO_AUTH, "not authorized to end shop" );

    int step = 0;
    vector<nasset> refund_nfts;

    nft_box_t::tbl_t nfts( get_self(), shop_id);
    auto itr = nfts.begin();
    CHECKC( itr != nfts.end(), err::RECORD_NOT_FOUND, "closed" )

    while( itr != nfts.end() ) {
        if (step > _gstate.max_step) return;

        refund_nfts.emplace_back( itr->nft );
        itr = nfts.erase( itr );
        step++;
    }

    if (itr != nfts.end()) {
        shop.nft_current_amount          -= step;
        shop.updated_at              = current_time_point();

        _db.set( shop );
    } else 
        _db.del( shop );

    if ( refund_nfts.size() > 0 )
        TRANSFER_N( shop.nft_contract, owner, refund_nfts, std::string("closeshop") )

}

void rndnft_mart::dealtrace(const deal_trace_s& trace) {
    require_auth(get_self());
    require_recipient(trace.buyer);
}

void rndnft_mart::_one_nft( const name& owner, shop_t& shop, nasset& nft ) {
    nft_box_t::tbl_t nfts( get_self(), shop.id) ;
    auto itr = nfts.begin();
    
    if ( shop.nft_current_amount > 1) {
        uint64_t rand = _rand( 1, shop.nft_current_amount, owner, shop.id );
        advance( itr , rand - 1 );
    } 
   
    nft = nasset( 1, itr->nft.symbol );
 
    if( itr->nft.amount <= 1 ) {
        nfts.erase( itr );
        shop.nft_current_amount     -= 1;

    } else {
        nfts.modify( itr, same_payer , [&]( auto& row ) {
            row.nft.amount -= 1;
        });
    }

    _db.set( shop );
}

uint64_t rndnft_mart::_rand(const uint16_t& min_unit, const uint64_t& max_uint, const name& owner, const uint64_t& shop_id) {
    auto mixid = tapos_block_prefix() * tapos_block_num() + owner.value + shop_id - current_time_point().sec_since_epoch();
    const char *mixedChar = reinterpret_cast<const char *>(&mixid);
    auto hash = sha256( (char *)mixedChar, sizeof(mixedChar));

    auto r1 = (uint64_t) hash.data()[0];
    uint64_t rand = r1 % max_uint + min_unit;
    return rand;
}

void rndnft_mart::_on_deal_trace(const deal_trace_s& deal_trace) {
    rndnft_mart::deal_trace_action act{ _self, { {_self, active_permission} } };
    act.send( deal_trace );
}