#include <nftone.mart/nftone.mart.hpp>
#include <amax.ntoken/amax.ntoken_db.hpp>
#include <amax.ntoken/amax.ntoken.hpp>
#include <cnyd.token/amax.xtoken.hpp>
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

      //reset with default
      // _gstate = global_t{};
   }

   /**
    * @brief send nasset tokens into nftone marketplace
    *
    * @param from
    * @param to
    * @param quantity
    * @param memo: $ask_price       E.g.:  10288/100    (its currency unit is CNYD)
    *               
    */
   void nftone_mart::onselltransfer(const name& from, const name& to, const vector<nasset>& quants, const string& memo) {
      CHECKC( from != to, err::ACCOUNT_INVALID, "cannot transfer to self" );

      if (from == get_self() || to != get_self()) return;
      
      CHECKC( memo != "", err::MEMO_FORMAT_ERROR, "empty memo!" )
      CHECKC( quants.size() == 1, err::OVERSIZED, "only one nft allowed to sell to nft at a timepoint" )
      float price             = 0.0f;
      compute_memo_price( memo, price );

      auto quant              = quants[0];
      CHECKC( quant.amount > 0, err::PARAM_ERROR, "non-positive quantity not allowed" )
      auto quantity           = quant;
      auto ask_price          = price_s(price, quant.symbol);
      auto earned             = asset(0, CNYD); //by seller
      auto bought             = nasset(0, quantity.symbol); //by buyer
      
      auto orders             = buyorder_idx( _self, quant.symbol.id );
      auto idx                = orders.get_index<"priceidx"_n>(); //larger first
      for (auto itr = idx.begin(); itr != idx.end(); itr++) {
         if (itr->price.value < ask_price.value) 
            break;   //offer or bit price < ask price
         
         auto offer_value     = itr->frozen;
         auto sell_value      = quantity.amount * itr->price.value;
         if (offer_value >= sell_value) {
            earned.amount     += sell_value;
            idx.modify(itr, same_payer, [&]( auto& row ) {
               row.frozen     -= sell_value;
            });
            
            //send to seller for CNYD tokens
            TRANSFER_X( CNYD_BANK, from, earned, "sell nft: " + to_string( quant.symbol.id ) )

            //send to buyer for NFT tokens
            bought.amount     = quantity.amount;
            vector<nasset> quants = { bought };
            TRANSFER_N( NFT_BANK, itr->maker, quants, "buy nft: " + to_string( quant.symbol.id ) )
            return;

         } else {// will execute the current offer wholely
            auto offer_amount  = itr->frozen / itr->price.value;
            earned.amount     += itr->frozen;
            quantity.amount   -= offer_amount;

            //send to buyer for nft tokens
            bought.amount     = offer_amount;
            vector<nasset> quants = { bought };
            TRANSFER_N( NFT_BANK, itr->maker, quants, "buy nft: " + to_string( quant.symbol.id) )

            idx.erase( itr );
         }
      }

      if (earned.amount > 0)
         TRANSFER_X( CNYD_BANK, from, earned, "sell nft: " + to_string( quant.symbol.id) )

      if (quantity.amount > 0) { //unsatisified remaining quantity will be placed as limit sell order
         auto sellorders = sellorder_idx( _self, quantity.symbol.id );
         sellorders.emplace(_self, [&]( auto& row ){
            row.id         = sellorders.available_primary_key(); if (row.id == 0) row.id = 1;
            row.price      = ask_price;
            row.frozen     = quantity.amount; 
            row.maker      = from;
            row.created_at = time_point_sec( current_time_point() );
         });
      }
   }

   /**
    * @brief send CNYD tokens into nftone marketplace to buy or place buy order
    *
    * @param from
    * @param to
    * @param quant
    * @param memo: t:$token_id:$bid_price | o:$token_id:$order_id:$bid_price
    *       E.g.:  t:123:10288/100           | o:123:1:10288/100
    */
   void nftone_mart::onbuytransfer(const name& from, const name& to, const asset& quant, const string& memo) {
      if (from == get_self() || to != get_self()) return;
      
      CHECKC( from != to, err::ACCOUNT_INVALID, "cannot transfer to self" );
      CHECKC( quant.amount > 0, err::PARAM_ERROR, "non-positive quantity not allowed" )
      CHECKC( memo != "", err::MEMO_FORMAT_ERROR, "empty memo!" )

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

      if (is_order_buy) {
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
                  row.frozen = order.frozen;
                  row.updated_at = current_time_point();
               });
            }
         } else {
            auto buyerbids          = buyer_bid_t::idx_t(_self, _self.value);
            auto id                 = buyerbids.available_primary_key();
            buyerbids.emplace(_self, [&]( auto& row ){
               row.id               = id;
               row.sell_order_id    = order_id;
               row.price            = bid_price;
               row.frozen           = quant.amount;
               row.created_at       = current_time_point();
            });

         }
      } else {
         compute_memo_price( string(params[2]), bid_price.value );

         auto idx                   = orders.get_index<"priceidx"_n>(); //smaller first
         for (auto itr = idx.begin(); itr != idx.end(); itr++) {
            auto order = *itr;
            if (order.price > bid_price)
               break;

            process_single_buy_order( order, quantity, bought );

            if (order.frozen == 0) {
               auto itr_del = itr;
               idx.erase( itr_del );

            } else {
               idx.modify(itr, same_payer, [&]( auto& row ) {
                  row.frozen = order.frozen;
                  row.updated_at = current_time_point();
               });
            }

            if (quantity.amount == 0)
               break;
         }
      }

      if (bought.amount > 0) {
         //send to buyer for nft tokens
         vector<nasset> quants = { bought };
         TRANSFER_N( NFT_BANK, from, quants, "buy nft: " + to_string(token_id) )
      }

      if (quantity.amount > 0) { //unsatisified remaining quantity will be placed as limit buy order
         auto buyorders = buyorder_idx( _self, token_id );
         buyorders.emplace(_self, [&]( auto& row ){
            row.id         = buyorders.available_primary_key(); if (row.id == 0) row.id = 1;
            row.price      = bid_price;
            row.frozen     = quantity.amount; 
            row.maker      = from;
            row.created_at = current_time_point();
         });
      }
   }

   //buyer to take a specific sell order
   ACTION nftone_mart::takeselorder( const name& issuer, const uint32_t& token_id, const uint64_t& sell_order_id ) {
      require_auth( issuer );
   }

   //seller to take a specific buy order
   /** disabled temporarily to cater for otc mode impl first
   ACTION nftone_mart::takebuyorder( const name& issuer, const uint32_t& token_id, const uint64_t& buy_order_id ) {
      require_auth( issuer );

      auto buyorders             = buyorder_idx( _self, token_id );
      auto buy_itr               = buyorders.find(buy_order_id);
      CHECKC( buy_itr            != buyorders.end(), err::RECORD_NOT_FOUND, "buy order not found: " + to_string(buy_order_id) )
      auto buyorder              = *buy_itr;
      auto buy_amount            = buyorder.frozen / buyorder.price.value;
      auto earned                = asset(0, CNYD); //to seller
      auto sellorders            = sellorder_idx( _self, token_id );
      auto sell_idx              = sellorders.get_index<"makerordidx"_n>();
      auto sold                  = nasset(0, buyorder.price.symbol); //by seller

      auto sell_itr_lower        = sell_idx.lower_bound( (uint128_t) issuer.value << 64 );
      auto sell_itr_upper        = sell_idx.upper_bound( (uint128_t) issuer.value << 64 | std::numeric_limits<uint64_t>::max() );
      for (auto sell_itr = sell_itr_lower; sell_itr != sell_itr_upper && sell_itr != sell_idx.end(); sell_itr++) {
         CHECKC( sell_itr->maker == issuer, err::NO_AUTH, "issuer not a seller: " + sell_itr->maker.to_string() )

         if (sold.amount + sell_itr->frozen > buy_amount) {
            sold.amount          = buy_amount;
            earned.amount        = buyorder.frozen;
            buyorder.frozen      = 0;
            sell_idx.modify(sell_itr, same_payer, [&]( auto& row ) {
               row.frozen        -= buy_amount - sold.amount;
               row.updated_at    = current_time_point();
            });
            break;

         } else {
            sold.amount          += sell_itr->frozen;
            earned.amount        += sell_itr->frozen * buyorder.price.value;
            buyorder.frozen      -= sell_itr->frozen * buyorder.price.value;

            auto sell_itr_del    = sell_itr;
            sell_idx.erase( sell_itr_del );
         }
      }

      if (buyorder.frozen == 0)
         buyorders.erase( buy_itr );

      if (sold.amount > 0) {
         //send to buyer for nft tokens
         vector<nasset> quants = { sold };
         TRANSFER_N( NFT_BANK, buyorder.maker, quants, "buy nft: " + to_string(token_id) )
      }

      //send to seller for quote tokens
      TRANSFER_X( CNYD_BANK, issuer, earned, "sell nft:" + to_string(sold.symbol.id) )
   } **/

   ACTION nftone_mart::takebuyorder( const name& issuer, const uint32_t& token_id, const uint64_t& buyer_bid_id ) {
      require_auth( issuer );

     
   }


/////////////////////////////// private funcs below /////////////////////////////////////////////

   void nftone_mart::process_single_buy_order( order_t& order, asset& quantity, nasset& bought ) {
      auto earned                = asset(0, CNYD); //to seller
      auto offer_cost            = order.frozen * order.price.value;
      if (offer_cost >= quantity.amount) {
         bought.amount           += quantity.amount / order.price.value;
         earned                  = quantity;
         order.frozen            -= bought.amount;
         quantity.amount         = 0;
         
         //send to seller for quote tokens
         TRANSFER_X( CNYD_BANK, order.maker, earned, "sell nft:" + to_string(bought.symbol.id) )

      } else {// will buy the current offer wholely and continue
         bought.amount           += order.frozen / order.price.value;
         earned.amount           = order.frozen;
         order.frozen            = 0;
         quantity.amount         -= offer_cost;

         TRANSFER_X( CNYD_BANK, order.maker, earned, "sell nft:" + to_string(bought.symbol.id) )
      }
   }

   void nftone_mart::cancelorder(const name& maker, const uint32_t& token_id, const uint64_t& order_id, const bool& is_sell_order) {
      // CHECKC( has_auth( maker ) || has_auth(_gstate.admin), err::NO_AUTH, "neither order maker nor admin" )

      if (is_sell_order) {
         auto orders = sellorder_idx(_self, token_id);
         if (order_id != 0) {
            auto itr = orders.find( order_id );
            CHECKC( itr != orders.end(), err::RECORD_NOT_FOUND, "order not exit: " + to_string(order_id) + "@" + to_string(token_id) )
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

      } else {
         auto orders = buyorder_idx(_self, token_id);
         if (order_id != 0) {
            auto itr = orders.find( order_id );
            CHECKC( itr != orders.end(), err::RECORD_NOT_FOUND, "order not exit: " + to_string(order_id) + "@" + to_string(token_id) )
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
   }

   void nftone_mart::compute_memo_price(const string& memo, float& price) {
      vector<string_view> params = split(memo, "//");
      switch (params.size()) {
         case 1:  price = (float) stoi( string(params[0]) ); break;
         case 2:  price = (float) stoi( string(params[0]) ) / (float) stoi( string(params[1]) ); break;
         default: CHECKC( false, err::MEMO_FORMAT_ERROR, "price format incorrect" )
      }
   }

} //namespace amax