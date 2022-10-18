#include <pass.mart/pass.mart.hpp>
#include <pass.mart/pass.mart.db.hpp>
#include <pass.mart/pass.mart.const.hpp>
#include <pass.custody/pass.custody.db.hpp>
#include <amax.ntoken/amax.ntoken.db.hpp>
#include <libraries/utils.hpp>
#include <set>

using namespace mart;
using namespace custody;
using namespace std;
using std::string;
using namespace amax;

namespace mart{
    struct account_t {
      asset balance;
      uint64_t primary_key() const { return balance.symbol.code().raw() ;}

      typedef eosio::multi_index< name("accounts"), account_t > idx_t;
    };
    

    inline void _check_min_amax( const name& account, const asset& min_amax ) {
        auto accts = account_t::idx_t(SYS_BANK, account.value);
        auto itr = accts.find(AMAX.code().raw());
        check( itr != accts.end(), "account has NO AMAX" );
        check( itr->balance >= min_amax, "account AMAX balance insufficient" );
    }

    inline void _check_nft( const name& nft_contract, const nsymbol& nft_symbol ) {
        nstats_t::idx_t nt( nft_contract, nft_contract.value );
        auto nt_itr = nt.find( nft_symbol.id);

        CHECKC( nt_itr != nt.end(), err::SYMBOL_MISMATCH,"nft not found, id:" + to_string(nft_symbol.id));
    }

    inline void _check_split_plan( const name& token_split_contract, const uint64_t& token_split_plan_id, const name& scope ) {
        split_plan_t::idx_t split_t( token_split_contract, scope.value );
        auto split_itr = split_t.find( token_split_plan_id );
        CHECKC( split_itr != split_t.end(), err::SYMBOL_MISMATCH,"token split plan not found, id:" + to_string(token_split_plan_id));
    }

    inline void _check_custody_plan( const name& custody_contract, const name& nft_contract, const nsymbol& nft_symbol, const uint64_t& custody_plan_id ) {
        plan_t::idx_t custody_plan( custody_contract, custody_contract.value );
        auto itr = custody_plan.find( custody_plan_id );
        CHECKC( itr != custody_plan.end(), err::SYMBOL_MISMATCH, "custody not found, id:" + to_string(custody_plan_id));
        CHECKC( itr->asset_contract == nft_contract, err::DATA_ERROR,"custody contract mismatch");
        CHECKC( itr->asset_symbol.id == nft_symbol.id, err::SYMBOL_MISMATCH,"custody asset symbol mismatch");
    }

    void pass_mart::init( const name& admin, const name& nft_contract, const name& gift_nft_contract, 
                            const name& custody_contract, const name& token_split_contract ) {
        require_auth( _self );

        _gstate.admin                   = admin;
        _gstate.nft_contract            = nft_contract;
        _gstate.gift_nft_contract       = gift_nft_contract;
        _gstate.custody_contract        = custody_contract;
        _gstate.token_split_contract    = token_split_contract;

    }

    void pass_mart::closepass( const uint64_t& pass_id) {
        auto prod = pass_t( pass_id );
        CHECKC( _db.get( prod ), err::RECORD_NOT_FOUND, "pass not found , id:" + to_string(pass_id) ) 
        CHECKC( prod.status == pass_status::open, err::STATUS_ERROR, "Abnormal status, status:" + prod.status.to_string() )
        CHECKC( has_auth(_self) || has_auth(_gstate.admin) || has_auth(prod.owner), err::NO_AUTH, "Missing required authority of admin or maintainer or owner" )

        auto now = current_time_point();
        prod.updated_at             = now;
        prod.sell_ended_at          = now;
        prod.status                 = pass_status::closed;

        _db.set( prod );
    }

    void pass_mart::setendtime( const uint64_t& pass_id, const time_point_sec& sell_ended_at ) {
        require_auth( _self );

        auto pass = pass_t( pass_id );
        CHECKC( _db.get( pass ), err::RECORD_NOT_FOUND, "pass not found , id:" + to_string(pass_id) ) 
        pass.sell_ended_at    = sell_ended_at;

        _db.set( pass );
    }

     void pass_mart::setowner( const uint64_t& pass_id, const name& owner ) {
         require_auth( _self );

        auto pass = pass_t( pass_id );
        CHECKC( _db.get( pass ), err::RECORD_NOT_FOUND, "pass not found , id:" + to_string(pass_id) ) 
        pass.owner          = owner;

        _db.set( pass );
     }


    void pass_mart::addpass( const name& owner, const string& title, const nsymbol& nft_symbol, const nsymbol& gift_symbol, 
                                const asset& price, const time_point_sec& started_at,
                                const time_point_sec& ended_at, uint64_t custody_plan_id,
                                const uint64_t& token_split_plan_id){

        CHECKC( has_auth(get_self()) || has_auth(_gstate.admin), err::NO_AUTH, "Missing required authority of admin or maintainer" );

        CHECKC( title.size() <= MAX_TITLE_SIZE, err::PARAM_ERROR, "title size must be <= " + to_string(MAX_TITLE_SIZE) );
        CHECKC( price.symbol.is_valid(), err::PARAM_ERROR, "Invalid asset symbol" );
        CHECKC( price.amount > 0, err::PARAM_ERROR, "Price must be greater than 0" );
        CHECKC( price.symbol == MUSDT, err::PARAM_ERROR, "The symbol must be: MUSDT" );
        CHECKC( ended_at > started_at, err::PARAM_ERROR, "End time must be greater than start time");
        CHECKC( is_account(owner),err::ACCOUNT_INVALID, "owner does not exist");
        CHECKC( is_account(_gstate.nft_contract), err::ACCOUNT_INVALID, "nft contract does not exist");
        CHECKC( is_account(_gstate.gift_nft_contract), err::ACCOUNT_INVALID, "gift contract does not exist");

        _check_nft( _gstate.nft_contract, nft_symbol );
        _check_nft( _gstate.gift_nft_contract, gift_symbol );
        _check_custody_plan( _gstate.custody_contract, _gstate.nft_contract, nft_symbol, custody_plan_id );        
        _check_split_plan( _gstate.token_split_contract, token_split_plan_id, _self );

        auto now = current_time_point();

        pass_t::idx_t passs( get_self(), get_self().value);
        passs.emplace( get_self(), [&]( auto& row ){
            row.id                          = ++_gstate.last_pass_id;
            row.title                       = title;
            row.owner                       = owner;
            row.nft_total                   = nasset(0, nft_symbol);
            row.nft_available               = nasset(0, nft_symbol);
            row.price                       = price;
            row.custody_plan_id             = custody_plan_id;
            row.status                      = pass_status::processing;
            row.gift_nft_total              = nasset(0, gift_symbol);
            row.gift_nft_available          = nasset(0, gift_symbol);
            row.token_split_plan_id         = token_split_plan_id;
            row.created_at                  = now;
            row.updated_at                  = now;
            row.sell_started_at             = started_at;
            row.sell_ended_at               = ended_at;
        });

    }

    /// @brief pass owner to transfer verso NFT into the contract as gift reward
    /// @param from 
    /// @param to 
    /// @param assets 
    /// @param memo - refuel:$pass_id, E.g. refuel:1
    void pass_mart::verso_transfer(const name& from, const name& to, const vector< nasset >& assets, const string& memo){
        if ( from == get_self() || to != get_self()) return;
        CHECKC( assets.size() == 1, err::OVERSIZED, "Only one NFT is allowed at a time" )

        vector<string_view> memo_params = split(memo, ":");
        ASSERT( memo_params.size() > 0 );
        CHECKC( _gstate.gift_nft_contract == get_first_receiver(), err::DATA_ERROR, "git nft contract incorrect" )
        CHECKC( memo_params[0] == "refuel", err::MEMO_FORMAT_ERROR, "memo prefix must be refuel" )
        CHECKC( memo_params.size() == 2 , err::MEMO_FORMAT_ERROR, "memo params size of must be 2" )

        nasset quantity                 = assets[0];
        auto pass                       = pass_t( std::stoul(string(memo_params[1])) );
        CHECKC( _db.get( pass ), err::RECORD_NOT_FOUND, "pass does not exist");
        // CHECKC( pass.status == pass_status::open, err::STATUS_ERROR, "Non open passs" );
        CHECKC( pass.owner == from, err::NO_AUTH, "Unauthorized");
        CHECKC( pass.gift_nft_available.symbol == quantity.symbol, err::SYMBOL_MISMATCH, "Pass Gift NFT Symbol mismatch" )
        
        pass.gift_nft_available         += quantity;
        pass.gift_nft_total             += quantity;
        pass.updated_at                 = current_time_point();

        _db.set( pass );
    }

    /// @brief pass owner to transfer NFTs into contract
    /// @param from 
    /// @param to 
    /// @param assets 
    /// @param memo  - refuel:${pass_id}, Eg: "refuel:1"
    void pass_mart::nft_transfer(const name& from, const name& to, const vector< nasset >& assets, const string& memo){
        if ( from == get_self() || to != get_self()) return;
        CHECKC( assets.size() == 1, err::OVERSIZED, "Only one NFT is allowed at a time point" );

        nasset quantity = assets[0];

        vector<string_view> memo_params = split(memo, ":");
        ASSERT(memo_params.size() > 0);
        auto now = time_point_sec(current_time_point());

        CHECKC( get_first_receiver() == _gstate.nft_contract, err::DATA_ERROR, "NFT contract must be " + _gstate.nft_contract.to_string() )
        CHECKC( memo_params[0] == "refuel", err::MEMO_FORMAT_ERROR, "memo prefix must be refuel" )
        CHECKC( memo_params.size() == 2, err::MEMO_FORMAT_ERROR, "pass params size must be 2" )

        auto pass_id = std::stoul( string(memo_params[1]) );
        auto pass = pass_t( pass_id );
        CHECKC( _db.get( pass ), err::RECORD_NOT_FOUND, "pass does not exist" )

        CHECKC( pass.owner == from, err::NO_AUTH, "Unauthorized operation" )
        CHECKC( pass.nft_available.symbol == quantity.symbol, err::SYMBOL_MISMATCH, "Pass NFT Symbol mismatch");

        pass.nft_available      += quantity;
        pass.nft_total          += quantity;
        pass.status             = pass_status::open;
        pass.updated_at         = now;

        _db.set( pass );
    }

    /// @brief - User to buy NFT Pass with MUSDT
    /// @param from 
    /// @param to 
    /// @param quantity 
    /// @param memo - buy:${pass_id}:${amount}, Eg:"buy:1:10"
    void pass_mart::mtoken_transfer(const name& from, const name& to, const asset& quantity, const string& memo){
        if ( from == get_self() || to != get_self()) return;

        CHECK( from != to, "cannot transfer to self" );
        CHECK( quantity.amount > 0, "quantity must be positive" );
        auto first_contract = get_first_receiver();
        CHECK( first_contract == MBANK, "The contract is not supported : " + first_contract.to_string() )

        _check_min_amax( from, asset(1'0000'0000, AMAX) );

        auto now = time_point_sec(current_time_point());

        vector<string_view> memo_params = split(memo, ":");
        ASSERT(memo_params.size() > 0);

        CHECKC( memo_params[0] == "buy", err::MEMO_FORMAT_ERROR, "memo format prefix not: buy" )
        CHECKC( memo_params.size() == 3 , err::MEMO_FORMAT_ERROR, "ontransfer:pass params size of must be 3" )

        uint64_t pass_id = std::stoul(string(memo_params[1]));
        uint64_t amount = std::stoul(string(memo_params[2]));
       
        auto pass = pass_t( pass_id );
        CHECKC( _db.get( pass ), err::RECORD_NOT_FOUND, "pass not found");
        CHECKC( pass.price.symbol == quantity.symbol, err::SYMBOL_MISMATCH, "pay symbol mismatch" )
        CHECKC( pass.status == pass_status::open, err::STATUS_ERROR, "pass not open" )
        CHECKC( pass.sell_started_at <= now, err::STATUS_ERROR, "sell not started yet" )
        CHECKC( pass.sell_ended_at >= now,err::STATUS_ERROR, "sell ended" )
        CHECKC( pass.price * amount == quantity, err::PARAM_ERROR, "transferred pay amount inconsistent with specified" )
        CHECKC( pass.nft_available.amount >= amount, err::OVERSIZED, "Insufficient Pass inventory, remaining: " + to_string( pass.nft_available.amount) )
        CHECKC( pass.gift_nft_available.amount >= amount, err::OVERSIZED, "Insufficient Gift inventory, remaining: " + to_string( pass.gift_nft_available.amount) )

        auto nft_quantity               = nasset( amount, pass.nft_total.symbol);
        auto gift_quantity              = nasset( amount, pass.gift_nft_total.symbol);
        pass.nft_available              -= nft_quantity;
        pass.gift_nft_available         -= gift_quantity;
        pass.updated_at                 = now;
        
	    _db.set( pass );
	
        order_s order;
        order.id                        = ++_gstate.last_order_id;
        order.pass_id                   = pass.id;
        order.owner                     = from;
        order.quantity                  = quantity;
        order.nft_quantity              = nft_quantity;
        order.gift_quantity             = gift_quantity;
        order.created_at                = now;
        _on_order_trace(order);

        vector<nasset> giftquants = { gift_quantity };
        TRANSFER( _gstate.gift_nft_contract, from, giftquants, std::string("MID") );
        
        vector<nasset> quants = { nft_quantity };
        TRANSFER( _gstate.nft_contract, _gstate.custody_contract, quants, std::string("lock:") + from.to_string() + ":" + to_string(pass.custody_plan_id) + ":0" )           

        TRANSFER( MBANK, _gstate.token_split_contract, quantity, std::string("plan:") + to_string( pass.token_split_plan_id) + ":" + to_string(amount) )
    }

    void pass_mart::ordertrace( const order_s& order){
        require_auth(get_self());
    }

    void pass_mart::_on_order_trace( const order_s& order) {
        pass_mart::order_srace_action act{ _self, { {_self, active_permission} } };
		act.send( order );
    }
}
