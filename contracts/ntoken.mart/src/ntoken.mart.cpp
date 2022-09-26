#include <amax.ntoken/amax.ntoken.hpp>
#include "ntoken.mart.hpp"
#include "commons/utils.hpp"

#include <cstdlib>
#include <ctime>

#include <chrono>

using std::chrono::system_clock;
using namespace wasm;
using namespace amax;
using namespace std;

#define TRANSFER(bank, to, quantity, memo) \
{ action(permission_level{get_self(), "active"_n }, bank, "transfer"_n, std::make_tuple( _self, to, quantity, memo )).send(); }


void blindbox::init( const name& owner){

    //CHECKC( false, err::MISC ,"error");
    require_auth( _self );
    _gstate.admin  = owner;
    _gstate.last_pool_id=0;
    _gstate.last_order_id=0;
    // pool_t::tbl_t pool( get_self(), get_self().value);
    // auto p_itr = pool.begin();
    // while( p_itr != pool.end() ){
    //     p_itr = pool.erase( p_itr );
    // } 
}

void blindbox::createpool( const name& owner,const string& title,const name& asset_contract, const name& blindbox_contract,
                           const asset& price, const name& fee_receiver,
                           const bool& allow_to_buy_again,
                           const time_point_sec& opended_at, const uint64_t& opened_days){
 
    CHECKC( has_auth(get_self()) || has_auth(_gstate.admin), err::NO_AUTH, "Missing required authority of admin or maintainer" );
    
    CHECKC( is_account(owner),err::ACCOUNT_INVALID,"owner does not exist");
    CHECKC( is_account(asset_contract),err::ACCOUNT_INVALID,"asset contract does not exist");
    CHECKC( is_account(fee_receiver),err::ACCOUNT_INVALID,"fee receiver does not exist");
    CHECKC( is_account(blindbox_contract),err::ACCOUNT_INVALID,"blinbox contract does not exist");
    
    CHECKC( price.amount > 0 , err::PARAM_ERROR ,"price must > 0");

    price_t pt;

    pt.value               = price;
    pt.received            = asset( 0 ,price.symbol);
    pt.fee_receiver        = fee_receiver;

    pool_t::tbl_t pool( get_self() ,get_self().value );
    pool.emplace( _self, [&](auto& row){
        row.id                  = ++_gstate.last_pool_id;
        row.owner               = owner;
        row.title               = title;
        row.price               = pt;
        row.asset_contract      = asset_contract;
        row.blindbox_contract   = blindbox_contract;
        row.allow_to_buy_again  = allow_to_buy_again;
        row.created_at          = current_time_point();
        row.updated_at          = current_time_point();
        row.opended_at          = opended_at;
        row.closed_at           = time_point_sec(opended_at.sec_since_epoch() + opened_days * DAY_SECONDS);
        row.status              = pool_status::enabled;
    });

    _global.set( _gstate, get_self() );
}

void blindbox::enableplan(const name& owner, const uint64_t& pool_id, bool enabled) {


    pool_t::tbl_t pool_tbl( get_self() ,get_self().value );
    auto pool_itr = pool_tbl.find(pool_id);
    CHECKC( pool_itr != pool_tbl.end(),  err::RECORD_NOT_FOUND,"pool not found: " + to_string(pool_id) )

    CHECKC( has_auth( owner ) || has_auth( _self ), err::NO_AUTH, "not authorized" )

    CHECKC( owner == pool_itr->owner,  err::NO_AUTH, "not authorized" )
    name new_status = enabled ? pool_status::enabled : pool_status::disabled ;
    CHECKC( pool_itr->status != new_status,err::STATUS_ERROR, "pool status is no changed" )

    pool_tbl.modify( pool_itr, same_payer, [&]( auto& pool ) {
        pool.status         = new_status;
        pool.updated_at     = current_time_point();
    });
}


void blindbox::editplantime(const name& owner, const uint64_t& pool_id, const time_point_sec& opended_at, const time_point_sec& closed_at){
    pool_t::tbl_t pool_tbl( get_self() ,get_self().value );
    auto pool_itr = pool_tbl.find(pool_id);
    CHECKC( pool_itr != pool_tbl.end(),  err::RECORD_NOT_FOUND,"pool not found: " + to_string(pool_id) )

    CHECKC( has_auth( owner ) || has_auth( _self ), err::NO_AUTH, "not authorized" )

    CHECKC( owner == pool_itr->owner,  err::NO_AUTH, "not authorized" )

    pool_tbl.modify( pool_itr, same_payer, [&]( auto& pool ) {
        pool.opended_at     = opended_at;
        pool.closed_at      = closed_at;
        pool.updated_at     = current_time_point();
    });
}


void blindbox::ontranstoken( const name& from, const name& to, const asset& quantity, const string& memo ){
    _on_open_transfer( from, to, quantity,memo);
}

void blindbox::ontransnft( const name& from, const name& to, const vector<nasset>& assets, const string& memo){

    _on_mint_transfer( from, to, assets, memo);
}

void blindbox::endpool(const name& owner, const uint64_t& pool_id){

    CHECKC( has_auth( owner ) || has_auth( _self ), err::NO_AUTH, "not authorized to end pool" )
    
    pool_t::tbl_t pool( get_self() ,get_self().value );
    auto pool_itr = pool.find( pool_id );

    CHECKC( pool_itr != pool.end(), err::RECORD_NOT_FOUND, "pool not found: " + to_string(pool_id) )
    CHECKC( pool_itr->status == pool_status::enabled, err::STATUS_ERROR, "pool not enabled, status:" + pool_itr->status.to_string() )
    CHECKC( pool_itr->owner == owner , err::NO_AUTH, "not authorized to end pool" );

    int step = 0;
    vector<nasset> quants;

    nft_boxes_t::tbl_t nft( get_self(), pool_id);
    auto itr = nft.begin();
    CHECKC( itr != nft.end(), err::RECORD_NOT_FOUND, "Completed");
    uint64_t amount = 0;
    while (itr != nft.end()) {

        if (step > 30) return;
        amount += itr->quantity.amount;

        if ( itr->transfer_type == transfer_type::transfer)
            quants.emplace_back( itr-> quantity);

        itr = nft.erase( itr );
        step++;
    }
    
    auto now = current_time_point();
    pool.modify( pool_itr, same_payer, [&]( auto& row ) {
        row.refund_nft_amount       += amount;
        row.updated_at              = now;
        row.max_table_distance      -= step;
    });
    if ( quants.size() > 0 ){
        TRANSFER( pool_itr->blindbox_contract, owner, quants , std::string("end pool"));
    }
}


void blindbox::_on_open_transfer( const name& from, const name& to, const asset& quantity, const string& memo){

    if (from == get_self() || to != get_self()) return;
    
    vector<string_view> memo_params = split(memo, ":");
    ASSERT(memo_params.size() > 0)

    auto now = time_point_sec(current_time_point());

    if ( memo_params[0] == "open" ){

        CHECKC( memo_params.size() == 2, err::MEMO_FORMAT_ERROR, "ontransfer:issue params size of must be 2")
        auto pool_id            = std::stoul(string(memo_params[1]));

        pool_t::tbl_t pool( get_self() ,get_self().value );
        auto pool_itr           = pool.find( pool_id );
        CHECKC( pool_itr != pool.end(), err::RECORD_NOT_FOUND, "pool not found: " + to_string(pool_id) )
        CHECKC( pool_itr->status == pool_status::enabled, err::STATUS_ERROR, "pool not enabled, status:" + pool_itr->status.to_string() )
        CHECKC( pool_itr->opended_at <= now, err::STATUS_ERROR, "Time is not up ");
        CHECKC( pool_itr->closed_at >= now,err::STATUS_ERROR, "It's too late ");

        auto amount = quantity.amount / pool_itr->price.value.amount;

        CHECKC( amount >= 1,err::PARAM_ERROR, "Minimum purchase quantity 1");
        CHECKC( amount * pool_itr->price.value.amount == quantity.amount , err::PARAM_ERROR ,"Quantity does not match price");
        CHECKC( pool_itr->asset_contract == get_first_receiver(), err::DATA_MISMATCH, "issue asset contract mismatch" );
        CHECKC( pool_itr->price.value.symbol == quantity.symbol, err::SYMBOL_MISMATCH, "blindbox asset symbol mismatch" );

        CHECKC( pool_itr->not_exchange_nft_amount >= amount ,  err::OVERSIZED, "Remaining blind box:" + to_string( pool_itr->not_exchange_nft_amount) );

        if ( !pool_itr ->allow_to_buy_again){
            buyer_t::tbl_t buyer( _self, pool_id);
            auto buyer_itr = buyer.find( from.value );
            CHECKC( buyer_itr == buyer.end(), err::RECORD_EXISTING,"Non repeatable purchase");
        }

        auto p = pool.get( pool_id );

        deal_trace_t trace;
        trace.pool_id = pool_id;
        trace.order_id = ++_gstate.last_order_id;
        trace.receiver = from;
        trace.pay_quantity = quantity;
        trace.created_at = now;
        trace.pay_contract  = pool_itr->asset_contract;
        _buy_one_nft( p , from, trace,amount);
        

        pool.modify( pool_itr, same_payer, [&]( auto& row ) {
            row.price.received              += quantity;
            row.not_exchange_nft_amount     = p.not_exchange_nft_amount;
            row.exchange_nft_amount         = p.exchange_nft_amount;
            row.updated_at                  = current_time_point();
            row.max_table_distance          = p.max_table_distance;
        });

        TRANSFER( pool_itr->asset_contract, pool_itr->price.fee_receiver, quantity , std::string(""));
    }
}

void blindbox::_on_mint_transfer( const name& from, const name& to, const vector<nasset>& assets, const string& memo ){
    if (from == get_self() || to != get_self()) return;
    require_auth( from );
    vector<string_view> memo_params = split(memo, ":");
    ASSERT(memo_params.size() > 0)

    CHECKC( assets.size() == 1, err::OVERSIZED, "Only one NFT is allowed at a time point" );
    nasset quantity = assets[0];
    auto now = time_point_sec(current_time_point());
    if (memo_params[0] == "mint") {

        CHECKC(memo_params.size() == 2, err::MEMO_FORMAT_ERROR, "ontransfer:issue params size of must be 2")
        auto pool_id            = std::stoul(string(memo_params[1]));

        pool_t::tbl_t pool( get_self() ,get_self().value );
        auto pool_itr           = pool.find( pool_id );
        CHECKC( pool_itr != pool.end(), err::RECORD_NOT_FOUND, "pool not found: " + to_string(pool_id) )
        CHECKC( pool_itr->status == pool_status::enabled, err::STATUS_ERROR, "pool not enabled, status:" + pool_itr->status.to_string() )
        CHECKC( pool_itr->blindbox_contract == get_first_receiver(), err::DATA_MISMATCH, "issue asset contract mismatch" );
        CHECKC( pool_itr->owner == from,  err::NO_AUTH, "not authorized" );
        CHECKC( pool_itr->total_nft_amount == 0, err::RECORD_EXISTING, "mint fail , total amount" + to_string(pool_itr->total_nft_amount));
        auto new_nft_id = pool_itr->last_nft_id;
        uint64_t max_distance = 0;
        uint64_t amount = quantity.amount;

        nft_boxes_t::tbl_t nft( get_self(), pool_id);
        nft.emplace( _self, [&]( auto& row ) {
            row.id                 = ++new_nft_id;
            row.quantity           = quantity;    
            row.transfer_type      = transfer_type::transfer;          
        });
          

        auto now = current_time_point();
        pool.modify( pool_itr, same_payer, [&]( auto& row ) {
            row.total_nft_amount            += amount;
            row.not_exchange_nft_amount     += amount;
            row.max_table_distance          = 1;
            row.last_nft_id                 = new_nft_id;
            row.updated_at                  = now;
        });

    }else{
        CHECKC( false,err::MISC,"not supported");
    }
}

void blindbox::dealtrace(const deal_trace_t& trace){

    require_auth(get_self());
    require_recipient(trace.receiver);

}

void blindbox::_buy_one_nft( pool_t& pool , const name& owner , deal_trace_t trace, const uint64_t& amount){

    nft_boxes_t::tbl_t nft( get_self(), pool.id);
    auto itr = nft.begin();
    CHECKC( itr != nft.end(), err::RECORD_NOT_FOUND, "No data available");
    CHECKC( itr->quantity.amount >= amount, err::OVERSIZED, "Insufficient remaining quantity");

    auto quantity = nasset( amount, itr-> quantity.symbol );
    vector<nasset> quants = { quantity };

    TRANSFER( pool.blindbox_contract, owner, quants , std::string("get nft"));
    trace.recv_quantity = quantity;
    trace.recv_contract = pool.blindbox_contract;
    
    if ( itr->quantity.amount == amount){

        nft.erase( itr );
        pool.max_table_distance --;

    }else {
        
        nft.modify( itr ,same_payer, [&]( auto& row){
            row.quantity.amount -= amount;
        });
    }
   
    pool.not_exchange_nft_amount     -= amount;
    pool.exchange_nft_amount         += amount;

    _add_times( pool.id, owner);
    _on_deal_trace(trace);
}

void blindbox::_add_times( const uint64_t& pool_id, const name& owner){

    buyer_t::tbl_t buyer( get_self() ,pool_id );
    auto itr = buyer.find( owner.value);
    auto now = current_time_point();
    if ( itr == buyer.end()){

        buyer.emplace( _self, [&]( auto& row ) {
            row.updated_at              = now;
            row.buy_times               = 1;
            row.created_at              = now;
            row.owner                   = owner;
        });

    }else {

        buyer.modify( itr, same_payer, [&]( auto& row ) {
            row.updated_at              = now;
            row.buy_times               += 1;
        });

    }
}


void blindbox::_on_deal_trace(const deal_trace_t& deal_trace)
{
    blindbox::deal_trace_action act{ _self, { {_self, active_permission} } };
        act.send( deal_trace );
}