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

      _gstate.dev_fee_collector = "amax.daodev"_n;
      _gstate.dev_fee_rate = 0.001f;
   }

   /**
    * @brief send nasset tokens into nftone marketplace
    *
    * @param from
    * @param to
    * @param quantity
    * @param memo: $ask_price       E.g.:  102.88    (its currency unit is CNYD)
    *               
    */
   void nftone_mart::onselltransfer(const name& from, const name& to, const nasset& quant, const string& memo) {
      CHECKC( from != to, err::ACCOUNT_INVALID, "cannot transfer to self" );
      CHECKC( quant.amount > 0, err::PARAM_ERROR, "non-positive quantity not allowed" )
      CHECKC( memo != "", err::MEMO_FORMAT_ERROR, "empty memo!" )

      auto quantity           = quant;
      auto ask_price          = price_s( quant.symbol, stof(memo) );
      auto earned             = asset(0, CNYD); //by seller
      auto bought             = nasset(0, quantity.symbol); //by buyer
      
      auto orders = buyorder_idx( _self, quant.symbol.id );
      auto idx = orders.get_index<"priceidx"_n>(); //larger first
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
            XTRANSFER( CNYD_BANK, from, earned, "sell nft: " + to_string( quant.symbol.id ) )

            //send to buyer for NFT tokens
            bought.amount     = quantity.amount;
            vector<nasset> quants = { bought };
            NTRANSFER( NFT_BANK, itr->maker, quants, "buy nft: " + to_string( quant.symbol.id ) )
            return;

         } else {// will execute the current offer wholely
            auto offer_amount  = itr->frozen / itr->price.value;
            earned.amount     += itr->frozen;
            quantity.amount   -= offer_amount;

            //send to buyer for nft tokens
            bought.amount     = offer_amount;
            vector<nasset> quants = { bought };
            NTRANSFER( NFT_BANK, itr->maker, quants, "buy nft: " + to_string( quant.symbol.id) )

            idx.erase( itr );
         }
      }

      if (earned.amount > 0)
         XTRANSFER( CNYD_BANK, from, earned, "sell nft: " + to_string( quant.symbol.id) )

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
    * @param memo: $tokenid:$bid_price       E.g.:  123:102.88
    */
   void nftone_mart::onbuytransfer(const name& from, const name& to, const asset& quant, const string& memo) {
      CHECKC( from != to, err::ACCOUNT_INVALID, "cannot transfer to self" );
      CHECKC( quant.amount > 0, err::PARAM_ERROR, "non-positive quantity not allowed" )
      CHECKC( memo != "", err::MEMO_FORMAT_ERROR, "empty memo!" )

      vector<string_view> params = split(memo, ":");
      auto param_size = params.size();
      CHECKC( param_size == 2, err::MEMO_FORMAT_ERROR, "memo format incorrect" )

      auto quantity           = quant;
      auto token_id           = stoi( string(params[0]) );
      auto nstats             = nstats_t::idx_t(NFT_BANK, NFT_BANK.value);
      auto nstats_itr         = nstats.find(token_id);
      CHECKC( nstats_itr      != nstats.end(), err::RECORD_NOT_FOUND, "nft token not found: " + to_string(token_id) )
      auto token_pid          = nstats_itr->supply.symbol.parent_id;
      auto nsymb              = nsymbol( token_id, token_pid );
      auto bid_price          = price_s( nsymb, stof( string(params[1] )));
      auto earned             = asset(0, CNYD); //by seller
      auto bought             = nasset(0, nsymb); //by buyer

      auto orders = sellorder_idx( _self, token_id );
      auto idx = orders.get_index<"priceidx"_n>(); //smaller first
      for (auto itr = idx.begin(); itr != idx.end(); itr++) {
         auto price_diff = itr->price.value - bid_price.value;
         if (price_diff > 0) 
            break;   //offer or ask price > bid price
         
         auto offer_cost = itr->price.value * itr->frozen;
         if (offer_cost >= quantity.amount) {
            auto offer_buy_amount = quantity.amount / itr->price.value;
            bought.amount += offer_buy_amount;

            idx.modify(itr, same_payer, [&]( auto& row ) {
               row.frozen -= offer_buy_amount;
               row.updated_at = time_point_sec( current_time_point() );
            });
            
            //send to buyer for nft tokens
            vector<nasset> quants = { bought };
            NTRANSFER( NFT_BANK, from, quants, "buy nft:" + to_string(token_id) )

            //send to seller for quote tokens
            earned.amount = quantity.amount;
            XTRANSFER( CNYD_BANK, itr->maker, earned, "sell nft:" + to_string(token_id) )
            return;

         } else {// will buy the current offer wholely and continue
            bought.amount += itr->frozen / itr->price.value;
            quantity.amount -= offer_cost;

            idx.erase( itr );

            earned.amount = itr->frozen;
            XTRANSFER( CNYD_BANK, itr->maker, earned, "sell nft:" + to_string(token_id) )
         }
      }

      if (bought.amount > 0) {
         vector<nasset> quants = { bought };
         NTRANSFER( NFT_BANK, from, quants, "buy nft: " + to_string(token_id) )
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

   void nftone_mart::cancelorder(const name& maker, const uint32_t& token_id, const uint64_t& order_id, const bool& is_sell_order) {
      require_auth( maker );

      if (is_sell_order) {
         auto orders = sellorder_idx(_self, token_id);
         if (order_id != 0) {
            auto itr = orders.find( order_id );
            CHECKC( itr != orders.end(), err::RECORD_NOT_FOUND, "order not exit: " + to_string(order_id) + "@" + to_string(token_id) )
            auto nft_quant = nasset( itr->frozen, itr->price.symbol );
            vector<nasset> quants = { nft_quant };
            NTRANSFER( NFT_BANK, itr->maker, quants, "nftone mart cancel" )
            orders.erase( itr );
         
         } else {
            for (auto itr = orders.begin(); itr != orders.end(); itr++) {
               auto nft_quant = nasset( itr->frozen, itr->price.symbol );
               vector<nasset> quants = { nft_quant };
               NTRANSFER( NFT_BANK, itr->maker, quants, "nftone mart cancel" )
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
            NTRANSFER( NFT_BANK, itr->maker, quants, "nftone mart cancel" )
            orders.erase( itr );
         
         } else {
            for (auto itr = orders.begin(); itr != orders.end(); itr++) {
               auto nft_quant = nasset( itr->frozen, itr->price.symbol );
               vector<nasset> quants = { nft_quant };
               NTRANSFER( NFT_BANK, itr->maker, quants, "nftone mart cancel" )
               orders.erase( itr );
            }
         }
      }
   }

} //namespace amax