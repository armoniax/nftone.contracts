#include <pass.mart/pass.mart.hpp>
#include <pass.mart/pass.mart_db.hpp>
#include <pass.mart/pass.mart_const.hpp>
#include <amax.ntoken/amax.ntoken_db.hpp>
#include <libraries/utils.hpp>
#include <set>

using namespace mart;
using namespace std;
using std::string;
using namespace amax;

namespace mart{

    void pass_mart::init(){

        require_auth( get_self() );

        CHECK( _gstate.started_at == time_point_sec(), "already init");

        _gstate.admin = get_self();
        
        _gstate.total_sells             = asset(0,MUSDT);
        _gstate.total_rewards           = asset(0,MUSDT);
        _gstate.total_claimed_rewards   = asset(0,MUSDT);

        _gstate.first_rate              = DEFAULT_FIRST_RATE;
        _gstate.second_rate             = DEFAULT_SECOND_RATE;
        _gstate.partner_rate            = DEFAULT_PARTNER_RATE;
        _gstate.claimrewrads_day        = DEFAULT_OPERABLE_DAYS;

        _gstate.last_product_id         = INITIAL_ID;
        _gstate.last_order_id           = INITIAL_ID;

        _gstate.started_at              = time_point_sec(current_time_point());
    }

    void pass_mart::setclaimday( const uint64_t& days){

        CHECK( _gstate.started_at != time_point_sec(), "initialization not complete");
        CHECK( has_auth(get_self()) || has_auth(_gstate.admin), "Missing required authority of admin or maintainer" )
        _gstate.claimrewrads_day        = days;

    }

    void pass_mart::setadmin(const name& newAdmin){

        CHECKC( _gstate.started_at != time_point_sec(),err::DATA_ERROR, "initialization not complete");
        CHECKC( has_auth(get_self()) || has_auth(_gstate.admin), err::NO_AUTH, "Missing required authority of admin or maintainer" )
        CHECKC( is_account(newAdmin), err::ACCOUNT_INVALID,"new admin does not exist");
        CHECKC( _gstate.admin != newAdmin, err::RECORD_EXISTING , "No change required" );
        _gstate.admin = newAdmin;
    }

    void pass_mart::setaccouts(const name& nft_contract,
                            const name& lock_contract,
                            const name& partner_account,
                            const name& storage_account,
                            const name& unable_claimrewards_account){
        
        CHECKC( _gstate.started_at != time_point_sec(),err::DATA_ERROR, "initialization not complete");
        CHECKC( has_auth(get_self()) || has_auth(_gstate.admin), err::NO_AUTH, "Missing required authority of admin or maintainer" );

        CHECKC( is_account( nft_contract ), err::ACCOUNT_INVALID,"nft contract does not exist");
        CHECKC( is_account( lock_contract ), err::ACCOUNT_INVALID, "lock contract does not exist");
        CHECKC( is_account( partner_account ), err::ACCOUNT_INVALID,"nft contract does not exist");  
        CHECKC( is_account( storage_account ), err::ACCOUNT_INVALID,"storage contract does not exist");  
        CHECKC( is_account( unable_claimrewards_account ), err::ACCOUNT_INVALID,"rewards collector does not exist");  

        _gstate.nft_contract = nft_contract;
        _gstate.lock_contract = lock_contract;
        _gstate.partner_account = partner_account;
        _gstate.storage_account = storage_account;
        _gstate.unable_claimrewards_account = unable_claimrewards_account;
    }

    void pass_mart::setrates(const uint64_t& first_rate,
                            const uint64_t& second_rate,
                            const uint64_t& partner_rate){

        CHECKC( _gstate.started_at != time_point_sec(),err::DATA_ERROR, "initialization not complete");
        CHECKC( has_auth(get_self()) || has_auth(_gstate.admin), err::NO_AUTH, "Missing required authority of admin or maintainer" );

        CHECKC( first_rate >= 0 && first_rate <= 10000 , err::PARAM_ERROR,"first rate range: 0-10000 ");
        CHECKC( second_rate >= 0 && second_rate <= 10000  , err::PARAM_ERROR,"second rate range: 0-10000 ");
        CHECKC( partner_rate >= 0 && partner_rate <= 10000  , err::PARAM_ERROR,"partner rate range: 0-10000 ");
        CHECKC( first_rate + second_rate + partner_rate <= 10000 , err::PARAM_ERROR, "total rate range: 0-10000");
        
        _gstate.first_rate = first_rate;
        _gstate.second_rate = second_rate;
        _gstate.partner_rate = partner_rate;
    }

    void pass_mart::setrule( const uint64_t& product_id,const rule_t& rule){
        
        product_t::tbl_t product( get_self(), get_self().value);
        auto itr = product.find(product_id);
        CHECKC( itr != product.end() , err::RECORD_NOT_FOUND, "product is not exist");
        CHECKC( has_auth(get_self()) || has_auth(_gstate.admin) || has_auth( itr->owner),err::NO_AUTH, "Missing required authority of admin or maintainer or owner" )
        CHECKC( rule.onetime_buy_amount >= 0, err::PARAM_ERROR,"The upper limit of one purchase is >= 0" );
        
        product.modify( itr,get_self(), [&]( auto& row){
            row.rule = rule;
        });
    }

    void pass_mart::cancelplan( const uint64_t& product_id){
        
        product_t::tbl_t product( get_self(), get_self().value);
        auto itr = product.find( product_id);
        CHECKC( itr != product.end(), err::RECORD_NOT_FOUND, "product not found , id:" + to_string(product_id));
        CHECKC( itr->status == product_status::opened, err::STATUS_ERROR, "Abnormal status, status:" + itr->status.to_string());
        CHECKC( has_auth(get_self()) || has_auth(_gstate.admin) || has_auth( itr->owner),err::NO_AUTH, "Missing required authority of admin or maintainer or owner" )

        auto now = time_point_sec(current_time_point());
        product.modify( itr, get_self(), [&]( auto&row ){
            row.updated_at                      = now;
            row.sell_ended_at                   = now;
            row.claimrewards_started_at         = now;
            row.claimrewards_ended_at           = time_point_sec(now.sec_since_epoch() + _gstate.claimrewrads_day * seconds_per_day);
            row.status                          = product_status::closed;
        });
    }

    // void pass_mart::editclaimtime( const uint64_t& product_id, const time_point_sec& ended_time){

    //     product_t::tbl_t product( get_self(), get_self().value);
    //     auto itr = product.find( product_id);
    //     CHECKC( itr != product.end(), err::RECORD_NOT_FOUND, "product not found , id:" + to_string(product_id));
    //     CHECKC( itr->status == product_status::opened, err::STATUS_ERROR, "Abnormal status, status:" + itr->status.to_string());
    //     CHECKC( has_auth(get_self()) || has_auth(_gstate.admin) || has_auth( itr->owner),err::NO_AUTH, "Missing required authority of admin or maintainer or owner" )
    //     CHECKC( itr->claimrewards_started_at < ended_time, err::PARAM_ERROR, "End time should be greater than start time");

    //     auto now = time_point_sec(current_time_point());
    //     product.modify( itr, get_self(), [&]( auto&row ){
    //         row.claimrewards_ended_at           = ended_time;
    //         row.updated_at                      = now;
    //     });

    // }
    void pass_mart::addproduct( const name& owner, const string& title, const nsymbol& nft_symbol,
                                const asset& price, const time_point_sec& started_at,
                                const time_point_sec& ended_at){

        CHECKC( _gstate.started_at != time_point_sec(),err::DATA_ERROR, "initialization not complete");
        CHECKC( has_auth(get_self()) || has_auth(_gstate.admin), err::NO_AUTH, "Missing required authority of admin or maintainer" );

        CHECKC( title.size() <= MAX_TITLE_SIZE, err::PARAM_ERROR, "title size must be <= " + to_string(MAX_TITLE_SIZE) );

        CHECKC( price.symbol.is_valid(), err::PARAM_ERROR, "Invalid asset symbol" );
        CHECKC( price.amount > 0, err::PARAM_ERROR,"Price should be greater than 0" );
        CHECKC( price.symbol == MUSDT, err::PARAM_ERROR,"The symbol should be: MUSDT" );

        CHECKC( ended_at > started_at, err::PARAM_ERROR, "End time should be greater than start time");
        
        CHECKC( is_account(owner),err::ACCOUNT_INVALID,"owner does not exist");
        CHECKC( is_account(_gstate.nft_contract), err::ACCOUNT_INVALID,"nft contract does not exist");
        // CHECKC( is_account(_gstate.lock_contract), err::ACCOUNT_INVALID, "lock contract does not exist");
        // CHECKC( is_account(_gstate.partner_account), err::ACCOUNT_INVALID,"partner account does not exist");  
        // CHECKC( is_account(_gstate.storage_account), err::ACCOUNT_INVALID,"unclaimed contract does not exist");  

        nstats_t::idx_t nt( _gstate.nft_contract, _gstate.nft_contract.value );
        auto nt_itr = nt.find( nft_symbol.id);
        CHECKC( nt_itr != nt.end(), err::SYMBOL_MISMATCH,"nft not found, id:" + to_string(nft_symbol.id));
        
        auto now = time_point_sec(current_time_point());

        product_t::tbl_t products( get_self(), get_self().value);

        _gstate.last_product_id ++;
        auto id = _gstate.last_product_id;;

        rule_t rule;

        products.emplace( get_self(), [&]( auto& row ){
            row.id                          = id;
            row.title                       = title;
            row.owner                       = owner;
            row.price                       = price;
            row.balance                     = nasset(0,nft_symbol);
            row.total_issue                 = nasset(0,nft_symbol);
            row.status                      = product_status::processing;
            row.rule                        = rule;
            row.created_at                  = now;
            row.updated_at                  = now;
            row.sell_started_at             = started_at;
            row.sell_ended_at               = ended_at;
            row.claimrewards_started_at     = ended_at;
            row.claimrewards_ended_at       = time_point_sec(ended_at.sec_since_epoch() + _gstate.claimrewrads_day * seconds_per_day);
        });
      
    }

    void pass_mart::nft_transfer(const name& from, const name& to, const vector< nasset >& assets, const string& memo){

        if ( from == get_self() || to != get_self()) return;
        //memo params format:
        //1. add:${product_id}, Eg: "add:1"

        CHECKC( assets.size() == 1, err::OVERSIZED, "Only one NFT is allowed at a time point" );

        nasset quantity = assets[0];

        vector<string_view> memo_params = split(memo, ":");
        ASSERT(memo_params.size() > 0);
        auto now = time_point_sec(current_time_point());

        auto first_contract = get_first_receiver();
        CHECK( first_contract == _gstate.nft_contract, "The contract is not supported : " + _gstate.nft_contract.to_string() )
        
        if ( memo_params[0] == "add" && first_contract== _gstate.nft_contract){

            CHECKC(memo_params.size() == 2 , err::MEMO_FORMAT_ERROR, "ontransfer:product params size of must be 2");
            
            product_t::tbl_t product( get_self(), get_self().value );

            uint64_t product_id = std::stoul(string(memo_params[1]));
            auto itr = product.find(product_id);
        
            CHECKC( itr != product.end() , err::RECORD_NOT_FOUND, "product is not exist");

            CHECKC( itr->balance.symbol == quantity.symbol, err::SYMBOL_MISMATCH, "Symbol mismatch");
            CHECKC( itr->status == product_status::processing, err::STATUS_ERROR, "Non processing products" );
            CHECKC( itr->owner == from, err::NO_AUTH, "Unauthorized operation");

            product.modify( itr, get_self() , [&]( auto& row ){
                
                row.balance += quantity;
                row.total_issue += quantity;
                row.updated_at = now;
                row.status = product_status::opened;
                
            }); 
        }
    }
    
   
    void pass_mart::token_transfer(const name& from, const name& to, const asset& quantity, const string& memo){

        if ( from == get_self() || to != get_self()) return;
        CHECK( from != to, "cannot transfer to self" );
        
        //memo params format:
        //1. buy:${product_id}:${amount}, Eg:"buy:1:10"
        CHECK( quantity.amount > 0, "quantity must be positive" );
        auto first_contract = get_first_receiver();
        CHECK( first_contract == BANK, "The contract is not supported : " + first_contract.to_string() )

        auto now = time_point_sec(current_time_point());

        vector<string_view> memo_params = split(memo, ":");
        ASSERT(memo_params.size() > 0);
        
        if ( memo_params[0] == "buy") {

            CHECKC(memo_params.size() == 3 , err::MEMO_FORMAT_ERROR, "ontransfer:product params size of must be 3")
            product_t::tbl_t product( get_self(), get_self().value );

            uint64_t product_id = std::stoul(string(memo_params[1]));
            auto itr = product.find(product_id);

            CHECKC( itr != product.end() , err::RECORD_NOT_FOUND, "product is not exist");
            CHECKC( itr->price.symbol == quantity.symbol, err::SYMBOL_MISMATCH, "Symbol mismatch");
            CHECKC( itr->status == product_status::opened, err::STATUS_ERROR, "Non open products" );
            CHECKC( itr->sell_started_at <= now, err::STATUS_ERROR, "Time is not up ");
            CHECKC( itr->sell_ended_at >= now,err::STATUS_ERROR, "It's too late ");

            uint64_t amount = std::stoul(string(memo_params[2]));
            CHECKC( itr->price.amount * amount == quantity.amount, err::PARAM_ERROR, "Inconsistent payment amount");
            CHECKC( itr->balance.amount >= amount , err::OVERSIZED, "Insufficient inventory , remaining:" + to_string( itr->balance.amount) );
            
            if ( itr->rule.onetime_buy_amount != 0){
                CHECKC( itr->rule.onetime_buy_amount >= amount, err::PARAM_ERROR, "The upper limit of a single purchase is " + to_string( itr->rule.onetime_buy_amount));
            }
            
            account_t::tbl_t account( get_self(), from.value);
            auto a_itr = account.find(product_id);
            if ( !itr->rule.allow_to_buy_again && a_itr != account.end() ){
                CHECKC( a_itr->pass.amount == 0, err::STATUS_ERROR, "Cannot continue to purchase");
            }
            
            auto nft_quantity = nasset(amount,itr->balance.symbol);
            product.modify( itr, get_self(), [&]( auto& row){
                row.balance         -= nft_quantity;
                row.updated_at      = now;
            }); 

            product_t pt = product.get(product_id);
            
            _add_quantity( pt, from, asset( 0, MUSDT), nft_quantity);
            _tally_rewards( pt, from, quantity, nft_quantity);

            vector<nasset> quants = { nft_quantity };

            TRANSFER( _gstate.nft_contract, _gstate.lock_contract, quants, std::string("lock:") + from.to_string() );

            _gstate.total_sells += quantity;
        }
    }

    void pass_mart::claimrewards( const name& owner, const uint64_t& product_id){
        
        require_auth( owner );
        product_t::tbl_t product( get_self() , get_self().value );
        auto p_itr = product.find( product_id );
        CHECKC( p_itr != product.end(), err::RECORD_NOT_FOUND, "product not found, id:" + to_string(product_id) );

        auto now = time_point_sec(current_time_point());
        CHECKC( p_itr->claimrewards_started_at <= now, err::STATUS_ERROR, "It's too early");
        CHECKC( p_itr->claimrewards_ended_at >= now,err::STATUS_ERROR, "It's too late");

        account_t::tbl_t account( get_self(), owner.value );
        auto a_itr = account.find( product_id );
        CHECKC( a_itr != account.end(), err::RECORD_NOT_FOUND, "RECORD_NOT_FOUND" ); 
        CHECKC( a_itr->status == account_status::ready, err::STATUS_ERROR, "Claim failed,status:" + a_itr->status.to_string());
        CHECKC( a_itr->sum_balance.amount > 0, err::DATA_ERROR, "rewards empty");

        auto quantity = a_itr->balance ;
        CHECKC( quantity.amount > 0 ,err::NOT_POSITIVE, "No claim");

        account.modify( a_itr, get_self(), [&]( auto& row ){
            row.updated_at      = now;
            row.status          = account_status::finished;
            row.balance         -= quantity;
        });
        
        TRANSFER( BANK, owner,quantity, std::string("claim"));
        _gstate.total_claimed_rewards += quantity;
    }
    
    void pass_mart::dealtrace(const deal_trace& trace){
        
        require_auth(get_self());
        require_recipient(trace.receiver);
        
    }

    void pass_mart::ordertrace( const order_t& order){
        require_auth(get_self());
    }


    void pass_mart::_add_quantity(const product_t& product, const name& owner, const asset& quantity,const nasset& nft_quantity){
        
        auto now = time_point_sec(current_time_point());

        pass_recv_t::tbl_t pass( get_self(), get_self().value);
        auto pass_itr = pass.find( owner.value);

        if ( nft_quantity.amount > 0 && pass_itr == pass.end()){
            pass_itr = pass.emplace( get_self(), [&]( auto& row){
                row.owner = owner;
                row.created_at = now;
            });
        }
        
        auto status = pass_itr != pass.end() ? account_status::ready : account_status::none;
        
        account_t::tbl_t account( get_self(), owner.value);
        auto a_itr = account.find(product.id);
        if ( a_itr == account.end()){
                account.emplace( get_self(), [&]( auto& row){
                    row.product_id              = product.id;
                    row.sum_balance             = quantity;
                    row.balance                 = quantity;
                    row.pass                    = nft_quantity;
                    row.updated_at              = now;
                    row.created_at              = now;
                    row.status                  = status;
                    //row.claimrewards_started_at     = product.claimrewards_started_at;
                    //row.claimrewards_ended_at       = product.claimrewards_ended_at;
                });   
            }else {
                account.modify( a_itr, get_self(), [&]( auto& row){
                    row.balance         += quantity;
                    row.sum_balance     += quantity;
                    row.pass            += nft_quantity;
                    row.updated_at      = now;
                    row.status          = status;
                });
        }
    }

    void pass_mart::_tally_rewards( const product_t& product, const name& owner,const asset& quantity, const nasset& nft_quantity){
        
        _gstate.last_order_id++;
        auto order_id = _gstate.last_order_id;
        
        // order_t::tbl_t order( get_self() , get_self().value );
        // order.emplace( get_self(), [&]( auto& row ){
        //         row.id              = order_id;
        //         row.product_id      = product.id;
        //         row.owner           = owner;
        //         row.quantity        = quantity;
        //         row.nft_quantity    = nft_quantity; 
        //         row.created_at      = time_point_sec(current_time_point());
        // });
        order_t order;
        order.id                        = order_id;
        order.product_id                = product.id;
        order.owner                     = owner;
        order.quantity                  = quantity;
        order.nft_quantity              = nft_quantity;
        order.created_at                 = time_point_sec(current_time_point());

        _on_order_deal_trace(order);

        asset direct_quantity           = asset( quantity.amount * _gstate.first_rate / 10000, quantity.symbol);
        asset indirect_quantity         = asset( quantity.amount * _gstate.second_rate / 10000, quantity.symbol);
        asset partner_quantity          = asset( quantity.amount * _gstate.partner_rate / 10000 ,quantity.symbol);

        asset total_rewards = direct_quantity + indirect_quantity + partner_quantity;
        //CHECKC( total_rewards <= quantity, err::RATE_OVERLOAD, "Reward overflow");
        name direct_name = get_account_creator(owner);
        _creator_reward( product, owner, direct_name , direct_quantity, reward_type::direct);

        name indirect_name = get_account_creator(direct_name);
        _creator_reward( product, owner, indirect_name , indirect_quantity, reward_type::indirect);
       
        _on_deal_trace( product.id, owner,_gstate.partner_account, partner_quantity, reward_type::partner);
        if ( partner_quantity.amount > 0){
            TRANSFER( BANK, _gstate.partner_account, partner_quantity, std::string("partner reward"));    
        }
        
        auto storage_quantity = quantity - total_rewards;
        
        if ( storage_quantity.amount > 0){
            TRANSFER( BANK, _gstate.storage_account, storage_quantity , std::string(""));
        }

        _gstate.total_rewards += total_rewards;
    }


    void pass_mart::_creator_reward( const product_t& product, const name& buyer,const name& creator, const asset& quantity, const name& status ){
        
        if ( creator != name() && creator != TOP_ACCOUNT){

            _add_quantity( product, creator, quantity, nasset( 0, product.balance.symbol ) );
            _on_deal_trace( product.id, buyer ,creator, quantity, status );

        }else {

            if ( quantity.amount > 0 )
                TRANSFER( BANK, _gstate.unable_claimrewards_account, quantity, std::string("unclaimed"));
        }
    }

    void pass_mart::_on_deal_trace(const uint64_t&product_id, const name&buyer, const name&receiver, const asset& quantity,const name& type)
    {

        if ( quantity.amount == 0 ) return;

        deal_trace trace;
        trace.product_id            =  product_id;
        trace.order_id              =  _gstate.last_order_id;
        trace.buyer                 =  buyer;
        trace.receiver              =  receiver;
        trace.quantity              =  quantity;
        trace.type                  =  type;
        trace.created_at            =  current_time_point();
       
        pass_mart::deal_trace_action act{ _self, { {_self, active_permission} } };
			act.send( trace );

    }

    void pass_mart::_on_order_deal_trace( const order_t& order){
        
         pass_mart::order_trace_action act{ _self, { {_self, active_permission} } };
			act.send( order );
    }
    
}