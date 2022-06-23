#include <nftone.mart/nftone.mart.hpp>
#include <amax.ntoken/amax.ntoken_db.hpp>
#include <amax.ntoken/amax.ntoken.hpp>
#include <cnyd.token/amax.xtoken.hpp>
#include <amax.mtoken/amax.mtoken.hpp>
#include <utils.hpp>
namespace amax {

using namespace std;

#define CHECKC(exp, code, msg) \
   { if (!(exp)) eosio::check(false, string("$$$") + to_string((int)code) + string("$$$ ") + msg); }


   void nftone_mart::init() {
      require_auth( _self );

      _gstate.admin = "amax.daodev"_n;
      _gstate.dev_fee_collector = "amax.daodev"_n;
      _gstate.dev_fee_rate = 0.001;
      _gstate.quote_symbol = CNYD;

      //reset with default
      // _gstate = global_t{};
   }

   /**
    * @brief send nasset tokens into nftone marketplace
    *
    * @param from
    * @param to
    * @param quantity
    * @param memo: $sn:$ask_price       E.g.:  1001:10288/100    (its currency unit is CNYD)
    *               
    */
   void nftone_mart::onselltransfer(const name& from, const name& to, const vector<nasset>& quants, const string& memo) {

      CHECKC( from != to, err::ACCOUNT_INVALID, "cannot transfer to self" );

      if (from == get_self() || to != get_self()) return;
      
      CHECKC( memo != "", err::MEMO_FORMAT_ERROR, "empty memo!" )
      CHECKC( quants.size() == 1, err::OVERSIZED, "only one nft allowed to sell to nft at a timepoint" )
      float price             = 0.0f;
      
      vector<string_view> params = split(memo, ":");
      CHECKC( 2 == params.size(), err::PARAM_ERROR, "param error" );

      uint64_t sn = to_uint64( params[0], "order_sn");
      compute_memo_price( string(params[1]), price );
      
      auto quant              = quants[0];
      CHECKC( quant.amount > 0, err::PARAM_ERROR, "non-positive quantity not allowed" )
      auto ask_price          = price_s(price, quant.symbol);

      auto sellorders      = sellorder_idx( _self, quant.symbol.id );
      auto ordersn_index   = sellorders.get_index<"ordersn"_n>();
      const auto& itr 	   = ordersn_index.find(sn);
      CHECKC( itr == ordersn_index.end(), err::RECORD_EXISTING, "the record already exists" );

      sellorders.emplace(_self, [&]( auto& row ){
         row.id         = sellorders.available_primary_key(); if (row.id == 0) row.id = 1;
         row.sn         = sn;
         row.price      = ask_price;
         row.frozen     = quant.amount; 
         row.maker      = from;
         row.created_at = time_point_sec( current_time_point() );
      });
   }

   /**
    * @brief send CNYD tokens into nftone marketplace to buy or place buy order
    *
    * @param from
    * @param to
    * @param quant
    * @param memo: o:$token_id:$order_id:$bid_price
    *       E.g.:  o:123:1:10288/100
    */
   void nftone_mart::onbuytransfer(const name& from, const name& to, const asset& quant, const string& memo) {
      if (from == get_self() || to != get_self()) return;

      CHECKC( from != to, err::ACCOUNT_INVALID, "cannot transfer to self" );
      CHECKC( quant.amount > 0, err::PARAM_ERROR, "non-positive quantity not allowed" )
      CHECKC( memo != "", err::MEMO_FORMAT_ERROR, "empty memo!" )

      CHECKC( quant.symbol  == _gstate.quote_symbol, err::PARAM_ERROR, "not support the symbol" )


      vector<string_view> params = split(memo, ":");
      auto param_size            = params.size();
      auto magic_no              = string( string(params[0] ));
      CHECKC( magic_no == "t" && param_size == 3 || magic_no == "o" && param_size == 4, err::MEMO_FORMAT_ERROR, "memo format incorrect" )

      auto is_order_buy          = (magic_no == "o");  
      auto quantity              = quant;
      auto token_id              = stoi( string( params[1] ));

      auto nstats                = nstats_t::idx_t(NFT_BANK, NFT_BANK.value);
      auto nstats_itr            = nstats.find(token_id);
      CHECKC( nstats_itr         != nstats.end(), err::RECORD_NOT_FOUND, "nft token not found: " + to_string(token_id) )
      auto token_pid             = nstats_itr->supply.symbol.parent_id;
      auto nsymb                 = nsymbol( token_id, token_pid );
      auto orders                = sellorder_idx( _self, token_id );
      auto bought                = nasset(0, nsymb); //by buyer
      auto bid_price             = price_s(0, nsymb); 

      compute_memo_price( string(params[3]), bid_price.value );

      auto order_id           = stoi( string( params[2] ));
      auto itr                = orders.find( order_id );
      CHECKC( itr != orders.end(), err::RECORD_NOT_FOUND, "order not found: " + to_string(order_id) + "@" + to_string(token_id) )

      auto order = *itr;
      if (order.price <= bid_price) {
         process_single_buy_order( order, quantity, bought );

         if (order.frozen == 0) {
            orders.erase( itr );
         } else {
            orders.modify(itr, same_payer, [&]( auto& row ) {
               row.frozen  = order.frozen;
               row.updated_at = current_time_point();
            });
         }
      } else {
         auto buyerbids          = buyer_bid_t::idx_t(_self, _self.value);
         auto id                 = buyerbids.available_primary_key();
         auto frozen             = int(quant.amount / bid_price.value / 10000);
         buyerbids.emplace(_self, [&]( auto& row ){
            row.id               = id;
            row.sell_order_id    = order_id;
            row.price            = bid_price;
            row.frozen           = frozen;
            row.buyer            = from;
            row.created_at       = current_time_point();
         });
         quantity.amount         -= frozen * bid_price.value * 10000;
      }
     

      if (bought.amount > 0) {
         //send to buyer for nft tokens
         vector<nasset> quants = { bought };
         TRANSFER_N( NFT_BANK, from, quants, "buy nft: " + to_string(token_id) )
      }

      if (quantity.amount > 0) { 
         transfer_token( from, quantity, "nft buy left" );
      }
   }

  
   ACTION nftone_mart::takebuybid( const name& seller, const uint32_t& token_id, const uint64_t& buyer_bid_id ) {
      require_auth( seller );

      auto bids                     = buyer_bid_t::idx_t(_self, _self.value);
      auto bid_itr                  = bids.find( buyer_bid_id );
      auto bid_frozen               = bid_itr->frozen;
      auto bid_price                = bid_itr->price;
      CHECKC( bid_itr != bids.end(), err::RECORD_NOT_FOUND, "buyer bid not found: " + to_string( buyer_bid_id ))
      auto sell_order_id = bid_itr->sell_order_id;

      auto sellorders               = sellorder_idx( _self, token_id );
      auto sell_itr                 = sellorders.find( sell_order_id );
      CHECKC( sell_itr != sellorders.end(), err::RECORD_NOT_FOUND, "sell order not found: " + to_string( sell_order_id ))
      auto sell_frozen              = sell_itr->frozen;

      auto nstats                   = nstats_t::idx_t(NFT_BANK, NFT_BANK.value);
      auto nstats_itr               = nstats.find(token_id);
      CHECKC( nstats_itr            != nstats.end(), err::RECORD_NOT_FOUND, "nft token not found: " + to_string(token_id) )
      auto token_pid                = nstats_itr->supply.symbol.parent_id;
      auto nsymb                    = nsymbol( token_id, token_pid );
      auto bought                   = nasset(0, nsymb); //by buyer
      auto earned                   = asset( 0, CNYD );

      
      if(bid_frozen < sell_frozen){
         bought.amount = bid_frozen;
         sellorders.modify( sell_itr, same_payer, [&]( auto& row ){
            row.frozen              = sell_frozen - bid_frozen;
            row.updated_at          = current_time_point();
         });
      }else{
         if(bid_frozen > sell_frozen){
            auto left = asset( 0, CNYD );
            left.amount = (bid_frozen - sell_frozen) * bid_price.value * 10000;
            transfer_token( bid_itr->buyer, left , "take nft left" );
         }
         bought.amount = sell_frozen;
         sellorders.erase( sell_itr );
      }
      bids.erase( bid_itr );

      vector<nasset> quants         = { bought };
      TRANSFER_N( NFT_BANK, bid_itr->buyer, quants, "buy nft: " + to_string(token_id) )
      earned.amount = bought.amount * bid_price.value * 10000;
      transfer_token( sell_itr->maker, earned, "take nft bid" );
   }


   /////////////////////////////// private funcs below /////////////////////////////////////////////

   void nftone_mart::process_single_buy_order( order_t& order, asset& quantity, nasset& bought ) {
      CHECKC( quantity.symbol  == _gstate.quote_symbol, err::PARAM_ERROR, "not support the symbol" )

      auto earned                = asset(0, _gstate.quote_symbol); //to seller
      auto offer_cost            = order.frozen * order.price.value * 10000;
      if (offer_cost >= quantity.amount) {
         bought.amount           += quantity.amount / order.price.value / 10000;
         earned.amount            = bought.amount * order.price.value * 10000;
         order.frozen            -= bought.amount;
         quantity.amount         -= earned.amount;
         
         //send to seller for quote tokens
         transfer_token( order.maker, earned, "sell nft:" + to_string(bought.symbol.id) );

      } else {// will buy the current offer wholely and continue
         bought.amount           += order.frozen;
         earned.amount           = offer_cost;
         order.frozen            = 0;
         quantity.amount         -= offer_cost;

         transfer_token( order.maker, earned, "sell nft:" + to_string(bought.symbol.id) );
      }
   }

   void nftone_mart::cancelbid( const name& buyer, const uint64_t& buyer_bid_id ){
      require_auth( buyer );
      auto bids                     = buyer_bid_t::idx_t(_self, _self.value);
      auto bid_itr                  = bids.find( buyer_bid_id );
      auto bid_frozen               = bid_itr->frozen;
      auto bid_price                = bid_itr->price;
      CHECKC( bid_itr != bids.end(), err::RECORD_NOT_FOUND, "buyer bid not found: " + to_string( buyer_bid_id ))
      CHECKC( buyer == bid_itr->buyer, err::NO_AUTH, "NO_AUTH")

      auto left = asset( 0, CNYD );
      left.amount = bid_frozen * bid_price.value * 10000;
      transfer_token( bid_itr->buyer,left , "cancel" );
      bids.erase( bid_itr );
   }
   
   void nftone_mart::cancelorder(const name& maker, const uint32_t& token_id, const uint64_t& order_id) {

      auto orders = sellorder_idx(_self, token_id);
      if (order_id != 0) {
         auto itr = orders.find( order_id );
         CHECKC( itr != orders.end(), err::RECORD_NOT_FOUND, "order not exit: " + to_string(order_id) + "@" + to_string(token_id) )
         CHECKC( maker == itr->maker, err::NO_AUTH, "NO_AUTH")

         auto nft_quant = nasset( itr->frozen, itr->price.symbol );
         vector<nasset> quants = { nft_quant };
         TRANSFER_N( NFT_BANK, itr->maker, quants, "nftone mart cancel" )
         orders.erase( itr );
      
      } else {
         for (auto itr = orders.begin(); itr != orders.end(); itr++) {
            auto nft_quant = nasset( itr->frozen, itr->price.symbol );
            vector<nasset> quants = { nft_quant };
            TRANSFER_N( NFT_BANK, itr->maker, quants, "nftone mart cancel" )
            orders.erase( itr );
         }
      }
   }

   void nftone_mart::compute_memo_price(const string& memo_price, float& price) {
      vector<string_view> params = split(memo_price, "//");
      switch (params.size()) {
         case 1:  price = (float) stoi( string(params[0]) ); break;
         case 2:  price = (float) stoi( string(params[0]) ) / (float) stoi( string(params[1]) ); break;
         default: CHECKC( false, err::MEMO_FORMAT_ERROR, "price format incorrect" )
      }
   }


   void nftone_mart::transfer_token(const name &to, const asset &quantity, const string &memo) {
      if( quantity.symbol == CNYD ) {
         TRANSFER_X( CNYD_BANK, to, quantity, memo )
      } else {
         TRANSFER_M( MTOKEN_BANK, to, quantity, memo )
      }

   }


} //namespace amax