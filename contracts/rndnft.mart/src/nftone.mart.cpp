#include <amax.ntoken/amax.ntoken.hpp>
#include "rndnft.mart.hpp"
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


void rndnft_mart::init( const name& owner){

    //CHECKC( false, err::MISC ,"error");
    require_auth( _self );
    _gstate.admin  = owner;
    // pool_t::tbl_t pool( get_self(), get_self().value);
    // auto p_itr = pool.begin();
    // while( p_itr != pool.end() ){
    //     p_itr = pool.erase( p_itr );
    // } 
}

void rndnft_mart::createpool( const name& owner,const string& title,const name& asset_contract, const name& nft_contract,
                           const asset& price, const name& fee_receiver,
                           const bool& allow_to_buy_again,
                           const time_point_sec& opended_at, const uint64_t& opened_days){
 
    CHECKC( has_auth(get_self()) || has_auth(_gstate.admin), err::NO_AUTH, "Missing required authority of admin or maintainer" );
    
    CHECKC( is_account(owner),err::ACCOUNT_INVALID,"owner does not exist");
    CHECKC( is_account(asset_contract),err::ACCOUNT_INVALID,"asset contract does not exist");
    CHECKC( is_account(fee_receiver),err::ACCOUNT_INVALID,"fee receiver does not exist");
    CHECKC( is_account(nft_contract),err::ACCOUNT_INVALID,"blinbox contract does not exist");
    
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
        row.nft_contract   = nft_contract;
        row.allow_to_buy_again  = allow_to_buy_again;
        row.created_at          = current_time_point();
        row.updated_at          = current_time_point();
        row.opended_at          = opended_at;
        row.closed_at           = time_point_sec(opended_at.sec_since_epoch() + opened_days * DAY_SECONDS);
        row.status              = pool_status::enabled;
    });

    _global.set( _gstate, get_self() );
}

void rndnft_mart::enableplan(const name& owner, const uint64_t& pool_id, bool enabled) {


    pool_t::tbl_t pool_tbl( get_self() ,get_self().value );
    auto pool_itr = pool_tbl.find(pool_id);
    CHECKC( pool_itr != pool_tbl.end(),  err::RECORD_NOT_FOUND,"pool not found: " + to_string(pool_id) )

    CHECKC( has_auth( owner ) || has_auth( _self ) || has_auth(_gstate.admin), err::NO_AUTH, "not authorized" )

    CHECKC( owner == pool_itr->owner,  err::NO_AUTH, "not authorized" )
    name new_status = enabled ? pool_status::enabled : pool_status::disabled ;
    CHECKC( pool_itr->status != new_status,err::STATUS_ERROR, "pool status is no changed" )

    pool_tbl.modify( pool_itr, same_payer, [&]( auto& pool ) {
        pool.status         = new_status;
        pool.updated_at     = current_time_point();
    });
}


void rndnft_mart::editplantime(const name& owner, const uint64_t& pool_id, const time_point_sec& opended_at, const time_point_sec& closed_at){
    pool_t::tbl_t pool_tbl( get_self() ,get_self().value );
    auto pool_itr = pool_tbl.find(pool_id);
    CHECKC( pool_itr != pool_tbl.end(),  err::RECORD_NOT_FOUND,"pool not found: " + to_string(pool_id) )

    CHECKC( has_auth( owner ) || has_auth( _self ) || has_auth(_gstate.admin), err::NO_AUTH, "not authorized" )

    CHECKC( owner == pool_itr->owner,  err::NO_AUTH, "not authorized" )

    pool_tbl.modify( pool_itr, same_payer, [&]( auto& pool ) {
        pool.opended_at     = opended_at;
        pool.closed_at      = closed_at;
        pool.updated_at     = current_time_point();
    });
}


void rndnft_mart::ontranstoken( const name& from, const name& to, const asset& quantity, const string& memo ){
    _on_open_transfer( from, to, quantity,memo);
}

void rndnft_mart::ontransnft( const name& from, const name& to, const vector<nasset>& assets, const string& memo){

    _on_fill_transfer( from, to, assets, memo);
}

void rndnft_mart::endpool(const name& owner, const uint64_t& pool_id){

    CHECKC( has_auth( owner ) || has_auth( _self ) || has_auth(_gstate.admin), err::NO_AUTH, "not authorized to end pool" )
    
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
        TRANSFER_N( pool_itr->nft_contract, owner, quants , std::string("end pool"));
    }
}

void rndnft_mart::fillnftinc( const name& owner, const uint64_t& pool_id, const uint64_t& begin_id, const uint64_t& end_id){
    
    CHECKC( has_auth( owner ) || has_auth( _self ) || has_auth(_gstate.admin), err::NO_AUTH, "not authorized" )
    pool_t::tbl_t pool_tbl( get_self() ,get_self().value );
    auto pool_itr = pool_tbl.find(pool_id);

    CHECKC( pool_itr != pool_tbl.end(),  err::RECORD_NOT_FOUND,"pool not found: " + to_string(pool_id) )
    CHECKC( owner == pool_itr->owner,  err::NO_AUTH, "not authorized" )
    CHECKC( pool_itr->status == pool_status::enabled, err::STATUS_ERROR, "pool not enabled, status:" + pool_itr->status.to_string() )

    auto p = pool_tbl.get(pool_id);

    vector<nasset> quants;
    account_t::idx_t account_tbl( p.nft_contract, p.owner.value );

    for ( auto i = begin_id; i <= end_id; i ++){
        
        auto account_itr = account_tbl.find( i );
        CHECKC( account_itr != account_tbl.end(),  err::RECORD_NOT_FOUND,"balance not found: " + to_string(i) )
        CHECKC( account_itr->balance.amount > 0 , err::OVERDRAWN,"overdrawn balance , id:" + to_string(i));
        quants.push_back(nasset(1, account_itr->balance.symbol));
    }
    
    _add_nfts( p, quants );

    pool_tbl.modify( pool_itr, same_payer, [&]( auto& pool ) {
        pool.total_nft_amount            = p.total_nft_amount;
        pool.not_exchange_nft_amount     = p.not_exchange_nft_amount;
        pool.max_table_distance          = p.max_table_distance;
        pool.last_nft_id                 = p.last_nft_id;        
        pool.updated_at                  = current_time_point();
    });
}

void rndnft_mart::fillnftids(const name& owner, const uint64_t& pool_id, const vector<nasset>& quants){
    CHECKC( has_auth( owner ) || has_auth( _self ) || has_auth(_gstate.admin), err::NO_AUTH, "not authorized" )
    pool_t::tbl_t pool_tbl( get_self() ,get_self().value );
    auto pool_itr = pool_tbl.find(pool_id);

    CHECKC( pool_itr != pool_tbl.end(),  err::RECORD_NOT_FOUND,"pool not found: " + to_string(pool_id) )
    CHECKC( owner == pool_itr->owner,  err::NO_AUTH, "not authorized" )
    CHECKC( pool_itr->status == pool_status::enabled, err::STATUS_ERROR, "pool not enabled, status:" + pool_itr->status.to_string() )
    
    auto p = pool_tbl.get(pool_id);
    
    account_t::idx_t account_tbl( p.nft_contract, p.owner.value );

    for( nasset quantity : quants){
        auto account_itr = account_tbl.find( quantity.symbol.id );
        CHECKC( account_itr != account_tbl.end(),  err::RECORD_NOT_FOUND,"balance not found: " + to_string(quantity.symbol.id) )
        CHECKC( account_itr->balance.amount > 0 , err::OVERDRAWN,"overdrawn balance , id:" + to_string(quantity.symbol.id));
    }

    _add_nfts( p, quants );

    pool_tbl.modify( pool_itr, same_payer, [&]( auto& pool ) {
        pool.total_nft_amount            = p.total_nft_amount;
        pool.not_exchange_nft_amount     = p.not_exchange_nft_amount;
        pool.max_table_distance          = p.max_table_distance;
        pool.last_nft_id                 = p.last_nft_id;        
        pool.updated_at                  = current_time_point();
    });
}
void rndnft_mart::dealtrace(const deal_trace_t& trace){

    require_auth(get_self());
    require_recipient(trace.receiver);

}

void rndnft_mart::_random_nft( pool_t& pool , const name& owner , deal_trace_t trace){

    nft_boxes_t::tbl_t nft( get_self(), pool.id);
    uint64_t rand = _rand( pool.max_table_distance , 1 , owner, pool.id);
    auto itr = nft.begin();
    advance( itr , rand - 1 );
    uint64_t amount = itr->quantity.amount;
    vector<nasset> quants = { itr->quantity };
    TRANSFER( pool.nft_contract, owner, quants , std::string("get rndnft_mart"));
    trace.recv_quantity = itr->quantity;
    trace.recv_contract = pool.nft_contract;
    nft.erase( itr );
    
    pool.max_table_distance --;
    pool.not_exchange_nft_amount     -= amount;
    pool.exchange_nft_amount         += amount;

    _add_times( pool.id, owner);
    _on_deal_trace(trace);
}


void rndnft_mart::_add_nfts( pool_t& p, const vector<nasset>& quants ){
    
    for( nasset quantity : quants){

        CHECKC( quantity.amount > 0, err::NOT_POSITIVE, "quantity must be positive" )

        p.max_table_distance++;
        p.total_nft_amount += quantity.amount;
        p.not_exchange_nft_amount += quantity.amount;

        nft_boxes_t::tbl_t nft( get_self(), p.id);
        nft.emplace( _self, [&]( auto& row ) {
            row.id                 = ++p.last_nft_id;
            row.quantity           = quantity;  
            row.transfer_type        = transfer_type::allowance;           
        });
        
    }
}

uint64_t rndnft_mart::_rand(uint64_t max_uint,  uint16_t min_unit, name owner , uint64_t pool_id){

    auto mixid = tapos_block_prefix() * tapos_block_num() + owner.value + pool_id - current_time_point().sec_since_epoch();
    const char *mixedChar = reinterpret_cast<const char *>(&mixid);
    auto hash = sha256( (char *)mixedChar, sizeof(mixedChar));

    auto r1 = (uint64_t)hash.data()[0];
    uint64_t rand = r1 % max_uint + min_unit;
    return rand;
}

void rndnft_mart::_on_open_transfer( const name& from, const name& to, const asset& quantity, const string& memo){

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

        CHECKC( amount == 1,err::PARAM_ERROR, "random type quantity must be 1");

        CHECKC( pool_itr->asset_contract == get_first_receiver(), err::DATA_MISMATCH, "issue asset contract mismatch" );
        CHECKC( pool_itr->price.value.symbol == quantity.symbol, err::SYMBOL_MISMATCH, "rndnft_mart asset symbol mismatch" );

        CHECKC( pool_itr->max_table_distance >= amount ,  err::OVERSIZED, "Remaining blind box:" + to_string( pool_itr->not_exchange_nft_amount) );

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
        _random_nft( p , from, trace);
        

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

void rndnft_mart::_on_fill_transfer( const name& from, const name& to, const vector<nasset>& assets, const string& memo ){
    if (from == get_self() || to != get_self()) return;
    require_auth( from );
    vector<string_view> memo_params = split(memo, ":");
    ASSERT(memo_params.size() > 0)

    auto now = time_point_sec(current_time_point());
    if (memo_params[0] == "fill" || memo_params[0] == "fill_loop") {

        CHECKC(memo_params.size() == 2, err::MEMO_FORMAT_ERROR, "ontransfer:issue params size of must be 2")
        auto pool_id            = std::stoul(string(memo_params[1]));

        pool_t::tbl_t pool( get_self() ,get_self().value );
        auto pool_itr           = pool.find( pool_id );
        CHECKC( pool_itr != pool.end(), err::RECORD_NOT_FOUND, "pool not found: " + to_string(pool_id) )
        CHECKC( pool_itr->status == pool_status::enabled, err::STATUS_ERROR, "pool not enabled, status:" + pool_itr->status.to_string() )
        //CHECKC( pool_itr->opended_at <= now, err::STATUS_ERROR, "Time is not up ");
        CHECKC( pool_itr->nft_contract == get_first_receiver(), err::DATA_MISMATCH, "issue asset contract mismatch" );
        // CHECKC( pool_itr->asset_symbol == quantity.symbol, err::SYMBOL_MISMATCH, "issue asset symbol mismatch" );
        CHECKC( pool_itr->owner == from,  err::NO_AUTH, "not authorized" );

        auto new_nft_id = pool_itr->last_nft_id;
        uint64_t max_distance = 0;
        uint64_t amount = 0;
        for( nasset quantity : assets){

            CHECKC( quantity.amount > 0, err::NOT_POSITIVE, "quantity must be positive" )

            amount += quantity.amount;

            if ( memo_params[0] == "fill_loop" ){

                max_distance += quantity.amount; 

                for ( auto i = 0; i < quantity.amount ; i++ ){
                    nft_boxes_t::tbl_t nft( get_self(), pool_id);
                    nft.emplace( _self, [&]( auto& row ) {
                        row.id                 = ++new_nft_id;
                        row.quantity           = nasset(1,quantity.symbol);  
                        row.transfer_type      = transfer_type::transfer;            
                    });
                }
            }else {
                max_distance ++;
                nft_boxes_t::tbl_t nft( get_self(), pool_id);
                nft.emplace( _self, [&]( auto& row ) {
                    row.id                 = ++new_nft_id;
                    row.quantity           = quantity;    
                    row.transfer_type      = transfer_type::transfer;          
                });
            }
        }

        auto now = current_time_point();
        pool.modify( pool_itr, same_payer, [&]( auto& row ) {
            row.total_nft_amount            += amount;
            row.not_exchange_nft_amount     += amount;
            row.max_table_distance          += max_distance;
            row.last_nft_id                 = new_nft_id;
            row.updated_at                  = now;
        });

    }
}
void rndnft_mart::_add_times( const uint64_t& pool_id, const name& owner){

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


void rndnft_mart::_on_deal_trace(const deal_trace_t& deal_trace)
{
    rndnft_mart::deal_trace_action act{ _self, { {_self, active_permission} } };
        act.send( deal_trace );
}