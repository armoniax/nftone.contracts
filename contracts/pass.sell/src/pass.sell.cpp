#include <pass.sell/pass.sell.hpp>
#include <pass.sell/pass.sell_db.hpp>
#include <pass.sell/pass.sell_const.hpp>
#include <amax.ntoken/amax.ntoken_db.hpp>
#include <thirdparty/utils.hpp>
#include <set>

using namespace sell;
using namespace std;
using std::string;
using namespace amax;

namespace sell{

    void pass_sell::init(){

        require_auth( get_self() );

        CHECK( _gstate.started_at == time_point_sec(), "already init");

        _gstate.admin = get_self();
        
        _gstate.total_sells             = asset(0,MUSDT);
        _gstate.total_rewards           = asset(0,MUSDT);
        _gstate.total_claimed_rewards   = asset(0,MUSDT);

        _gstate.first_rate              = DEFAULT_FIRST_RATE;
        _gstate.second_rate             = DEFAULT_SECOND_RATE;
        _gstate.partner_rate            = DEFAULT_PARTNER_RATE;
        _gstate.operable_days           = DEFAULT_OPERABLE_DAYS;

        _gstate.last_product_id         = INITIALIZED_ID;
        _gstate.last_order_id           = INITIALIZED_ID;

        _gstate.started_at              = time_point_sec(current_time_point());
    }

    void pass_sell::setadmin(const name& admin,const name& newAdmin){

       _check_conf_auth(admin);

        CHECKC( is_account(newAdmin), err::ACCOUNT_INVALID,"new admin does not exist");
        _gstate.admin = newAdmin;
    }

    void pass_sell::setaccouts(const name& owner,
                            const name& nft_contract,
                            const name& lock_contract,
                            const name& partner_account){
        
        _check_conf_auth(owner);

        CHECKC( is_account(nft_contract), err::ACCOUNT_INVALID,"nft contract does not exist");
        CHECKC( is_account(lock_contract), err::ACCOUNT_INVALID, "lock contract does not exist");
        CHECKC( is_account(partner_account), err::ACCOUNT_INVALID,"nft contract does not exist");  

        _gstate.nft_contract = nft_contract;
        _gstate.lock_contract = lock_contract;
        _gstate.partner_account = partner_account;
    }

    void pass_sell::setrates(const name& owner,
                            const uint64_t& first_rate,
                            const uint64_t& second_rate,
                            const uint64_t& partner_rate){

        _check_conf_auth( owner );
        
        CHECKC( first_rate >= 0 && first_rate <= 10000 , err::PARAM_ERROR,"first rate range: 0-10000 ");
        CHECKC( second_rate >= 0 && second_rate <= 10000  , err::PARAM_ERROR,"second rate range: 0-10000 ");
        CHECKC( partner_rate >= 0 && partner_rate <= 10000  , err::PARAM_ERROR,"partner rate range: 0-10000 ");
        CHECKC( first_rate + second_rate + partner_rate <= 10000 , err::PARAM_ERROR, "total rate range: 0-10000");
        
        _gstate.first_rate = first_rate;
        _gstate.second_rate = second_rate;
        _gstate.partner_rate = partner_rate;
    }

    void pass_sell::addproduct( const name& owner, const string& title, const nsymbol& nft_symbol,
                                const asset& price, const rule_t& rule, const time_point_sec& started_at,
                                const time_point_sec& ended_at){

        _check_conf_auth(_gstate.admin);

        CHECKC( title.size() <= MAX_TITLE_SIZE, err::PARAM_ERROR, "title size must be <= " + to_string(MAX_TITLE_SIZE) );

        CHECKC( price.symbol.is_valid(), err::PARAM_ERROR, "Invalid asset symbol" );
        CHECKC( price.amount > 0, err::PARAM_ERROR,"Price should be greater than 0" );
        CHECKC( price.symbol == MUSDT, err::PARAM_ERROR,"The symbol should be: MUSDT" );

        CHECKC( rule.single_amount > 0, err::PARAM_ERROR,"Single purchase quantity should be greater than 0" );

        CHECKC( ended_at > started_at, err::PARAM_ERROR, "End time should be greater than start time");

        CHECKC( is_account(_gstate.nft_contract), err::ACCOUNT_INVALID,"nft contract does not exist");
        CHECKC( is_account(_gstate.lock_contract), err::ACCOUNT_INVALID, "lock contract does not exist");
        CHECKC( is_account(_gstate.partner_account), err::ACCOUNT_INVALID,"partner account does not exist");  

        auto now = time_point_sec(current_time_point());

        product_t::tbl_t products( get_self(), get_self().value);
        auto id = _gstate.last_product_id;
        id++;
        _gstate.last_product_id = id;

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
            row.operable_started_at         = ended_at;
            row.operable_ended_at           = time_point_sec(ended_at.sec_since_epoch() + _gstate.operable_days * seconds_per_day);
        });
      
    }



    void pass_sell::nft_transfer(const name& from, const name& to, const vector< nasset >& assets, const string& memo){

        if ( from == get_self() || to != get_self()) return;
        //memo params format:
        //1. add:${product_id}, Eg: "add:1"

        CHECKC( assets.size() == 1, err::OVERSIZED, "only one nft allowed to sell to nft at a timepoint" );

        nasset quantity = assets[0];

        vector<string_view> memo_params = split(memo, ":");
        ASSERT(memo_params.size() > 0);
        auto now = time_point_sec(current_time_point());
        if ( memo_params[0] == "add" ){

            CHECKC(memo_params.size() == 2 , err::MEMO_FORMAT_ERROR, "ontransfer:product params size of must be 2")
            
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

    void pass_sell::token_transfer(const name& from, const name& to, const asset& quantity, const string& memo){

        if ( from == get_self() || to != get_self()) return;
        CHECK( from != to, "cannot transfer to self" );
        
        //memo params format:
        //1. buy:${product_id}:${amount}, Eg:"buy:1:10"
        CHECK( quantity.amount > 0, "quantity must be positive" );
        auto first_contract = get_first_receiver();
        CHECK( first_contract == BANK, "none MUSDT payment not allowed: " + first_contract.to_string() )

        auto now = time_point_sec(current_time_point());

        vector<string_view> memo_params = split(memo, ":");
        ASSERT(memo_params.size() > 0);
        
         if ( memo_params[0] == "buy" ) {

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
            
            CHECKC( itr->rule.single_amount >= amount, err::PARAM_ERROR, "The upper limit of a single purchase is " + to_string( itr->rule.single_amount));
            account_t::tbl_t account( get_self(), from.value);
            auto a_itr = account.find(product_id);
            if ( !itr->rule.again_flag ){
                CHECKC( a_itr != account.end() || a_itr->card.amount == 0, err::STATUS_ERROR, "Cannot continue to purchase");
            }
            
            auto nft_quantity = nasset(amount,itr->balance.symbol);
            product.modify( itr, get_self(), [&]( auto& row){
                row.balance         -= nft_quantity;
                row.updated_at      = now;
            }); 

            

            product_t pt = product.get(product_id);
            
            _add_card( pt, from, nft_quantity);
            _reward( pt, from, quantity, nft_quantity);

            vector<nasset> quants = { nft_quantity };

            TRANSFER( _gstate.nft_contract, _gstate.lock_contract, quants, std::string("lock"));

            _gstate.total_sells += quantity;
         }
    }

    void pass_sell::claimrewards( const name& owner, const uint64_t& product_id){
        
        require_auth( owner );
        product_t::tbl_t product( get_self() , get_self().value );
        auto p_itr = product.find( product_id );
        CHECKC( p_itr != product.end(), err::RECORD_NOT_FOUND, "RECORD_NOT_FOUND" );

        auto now = time_point_sec(current_time_point());
        CHECKC( p_itr->operable_started_at <= now, err::STATUS_ERROR, "It's too early");
        CHECKC( p_itr->operable_ended_at >= now,err::STATUS_ERROR, "It's too late");

        account_t::tbl_t account( get_self(), owner.value );
        auto a_itr = account.find( product_id );
        CHECKC( a_itr != account.end(), err::RECORD_NOT_FOUND, "RECORD_NOT_FOUND" ); 
        CHECKC( a_itr->balance.amount > 0, err::NOT_POSITIVE, "No claim");
        CHECKC( a_itr->status == account_status::ready, err::STATUS_ERROR, "No card");

        
        TRANSFER( BANK, owner,a_itr->balance, std::string("claim"));

        _gstate.total_claimed_rewards += a_itr->balance;

        account.modify( a_itr, get_self(), [&]( auto& row ){
            row.balance.amount  = 0;
            row.updated_at      = now;
            row.status          = account_status::finished;
        });
        
        
    }
    
    void pass_sell::_reward(const product_t& product, const name& owner,const asset& quantity, const nasset& nft_quantity){

        asset first_quantity     = asset( quantity.amount * _gstate.first_rate / 10000, quantity.symbol);
        asset second_quantity    = asset( quantity.amount * _gstate.second_rate / 10000, quantity.symbol);
        asset partner_quantity   = asset( quantity.amount * _gstate.partner_rate / 10000 ,quantity.symbol);

        CHECKC( first_quantity + second_quantity + partner_quantity < quantity, err::RATE_OVERLOAD, "Reward overflow");
        
        name first_name = get_account_creator(owner);
        name second_name = get_account_creator(first_name);

        _add_balance( product, first_name, first_quantity);
        _add_balance( product, second_name, second_quantity);

        _gstate.last_order_id++;
        auto order_id = _gstate.last_order_id;
        
        order_t::tbl_t order( get_self() , get_self().value );
        order.emplace( get_self(), [&]( auto& row ){
                row.id              = order_id;
                row.product_id      = product.id;
                row.owner           = owner;
                row.quantity        = quantity;
                row.nft_quantity    = nft_quantity; 
                row.created_at      = time_point_sec(current_time_point());
                row.rewards         = {
                    reward_t( first_name  ,first_quantity,  reward_type::direct),
                    reward_t( second_name  ,second_quantity,  reward_type::indirect)
                };
        });

        

        TRANSFER( BANK, _gstate.partner_account, partner_quantity, std::string("partner reward"));

        _gstate.total_rewards += first_quantity + second_quantity + partner_quantity;
    }

    void pass_sell::_add_card( const product_t& product, const name& owner, const nasset& nft_quantity){
        
        uint64_t product_id = product.id;
        auto now = time_point_sec(current_time_point());

        account_t::tbl_t account( get_self(), owner.value);
        auto a_itr = account.find(product_id);
        if ( a_itr == account.end()){
                account.emplace( get_self(), [&]( auto& row){
                    row.product_id      = product_id;
                    row.balance         = asset(0,MUSDT);
                    row.card            = nft_quantity;
                    row.updated_at      = now;
                    row.created_at      = now;
                    row.status          = account_status::ready;
                });   
            }else {
                account.modify( a_itr, get_self(), [&]( auto& row){
                    row.card            += nft_quantity;
                    row.status          = account_status::ready;
                    row.updated_at      = now;
            });
        }

        card_t::tbl_t card( get_self(), get_self().value);
        auto card_itr = card.find( owner.value);
        if (card_itr == card.end()){
            card.emplace( get_self(), [&]( auto& row){

                row.owner = owner;
                row.created_at = now;
            });
        }
    }

    void pass_sell::_add_balance(const product_t& product, const name& owner, const asset& quantity){
        
        card_t::tbl_t card( get_self(), get_self().value);
        auto card_itr = card.find( owner.value);

        auto status = card_itr == card.end() ? account_status::none : account_status::ready;

        auto now = time_point_sec(current_time_point());

        account_t::tbl_t account( get_self(), owner.value);
        auto a_itr = account.find(product.id);
        if ( a_itr == account.end()){
                account.emplace( get_self(), [&]( auto& row){
                    row.product_id          = product.id;
                    row.balance             = quantity;
                    row.card                = nasset( 0, product.balance.symbol);
                    row.updated_at          = now;
                    row.created_at          = now;
                    row.status              = status;
                    row.operable_started_at = product.operable_started_at;
                    row.operable_ended_at   = product.operable_ended_at;
                });   
            }else {
                account.modify( a_itr, get_self(), [&]( auto& row){
                    row.balance         += quantity;
                    row.updated_at      = now;
                    row.status = status;
                });
            }
    }

    void pass_sell::_check_conf_auth(const name& owner){
          
        CHECK( _gstate.started_at != time_point_sec(), "initialization not complete");
        CHECK( owner == _gstate.admin, "owner mismatch" );
        CHECK( has_auth(get_self()) || has_auth(_gstate.admin), "Missing required authority of admin or maintainer" )
    }
    
}