#include <nftone.mart/nftone.mart.hpp>
#include <amax.ntoken/amax.ntoken_db.hpp>
#include <amax.ntoken/amax.ntoken.hpp>
#include <aplink.farm/aplink.farm.hpp>
#include <cnyd.token/amax.xtoken.hpp>

#include<math.hpp>

#include <utils.hpp>

static constexpr eosio::name active_permission{"active"_n};
static constexpr symbol   APL_SYMBOL          = symbol(symbol_code("APL"), 4);

#define ALLOT_APPLE(farm_contract, lease_id, to, quantity, memo) \
    {   aplink::farm::allot_action(farm_contract, { {_self, active_perm} }).send( \
            lease_id, to, quantity, memo );}

namespace amax {

using namespace std;

#define CHECKC(exp, code, msg) \
   { if (!(exp)) eosio::check(false, string("$$$") + to_string((int)code) + string("$$$ ") + msg); }


   inline int64_t get_precision(const symbol &s) {
      int64_t digit = s.precision();
      CHECK(digit >= 0 && digit <= 18, "precision digit " + std::to_string(digit) + " should be in range[0,18]");
      return calc_precision(digit);
   }


   void nftone_mart::init(const symbol& pay_symbol, const name& bank_contract, const name& admin,
                              const double& devfeerate, const name& feecollector,
                              const double& ipfeerate ) {
      require_auth( _self );

      CHECKC( devfeerate > 0.0 && devfeerate < 1.0, err::PARAM_ERROR, "devfeerate needs to be between 0 and 1" )
      CHECKC( ipfeerate > 0.0 && ipfeerate < 1.0, err::PARAM_ERROR, "ipfeerate to be between 0 and 1" )
      CHECKC( ipfeerate + devfeerate < 1.0, err::PARAM_ERROR, "The handling ipfeerate plus devfeerate shall not exceed 1" )
      CHECKC( is_account(feecollector), err::ACCOUNT_INVALID, "feecollector cannot be found" )
      CHECKC( is_account(admin), err::ACCOUNT_INVALID, "admin cannot be found" )

      _gstate.admin                 = admin;
      _gstate.dev_fee_collector     = feecollector;
      _gstate.dev_fee_rate          = devfeerate;
      _gstate.ipowner_fee_rate      = ipfeerate;
      _gstate.pay_symbol            = pay_symbol;
      _gstate.bank_contract         = bank_contract;

      _gstate.last_buy_order_idx    = 14000;
      _gstate.last_deal_idx         = 69;
      // _gstate.order_expiry_hours    = 72;

   }

   /**
    * @brief send nasset tokens into nftone marketplace
    *
    * @param from
    * @param to
    * @param quantity
    * @param memo: $ask_price       E.g.:  10288    (its currency unit is CNYD)
    *
    */
   void nftone_mart::onselltransfer(const name& from, const name& to, const vector<nasset>& quants, const string& memo) {
      CHECKC( from != to, err::ACCOUNT_INVALID, "cannot transfer to self" );

      if (from == get_self() || to != get_self()) return;

      CHECKC( memo != "", err::MEMO_FORMAT_ERROR, "empty memo!" )
      CHECKC( quants.size() == 1, err::OVERSIZED, "only one nft allowed to sell to nft at a timepoint" )
      asset price          = asset( 0, _gstate.pay_symbol );
      compute_memo_price( memo, price );

      auto quant              = quants[0];
      CHECKC( quant.amount > 0, err::PARAM_ERROR, "non-positive quantity not allowed" )
      auto ask_price          = price_s(price, quant.symbol);

      auto sellorders = sellorder_idx( _self, quant.symbol.id );
      _gstate.last_buy_order_idx ++;
      sellorders.emplace(_self, [&]( auto& row ) {
         row.id         =  _gstate.last_buy_order_idx;
         row.price      = ask_price;
         row.frozen     = quant.amount;
         row.maker      = from;
         row.created_at = time_point_sec( current_time_point() );
      });
   }


   void nftone_mart::onbuytransfercnyd(const name& from, const name& to, const asset& quant, const string& memo) {
      on_buy_transfer(from, to, quant, memo);
   }

   void nftone_mart::onbuytransfermtoken(const name& from, const name& to, const asset& quant, const string& memo) {
      on_buy_transfer(from, to, quant, memo);
   }

   /**
    * @brief send CNYD/MUSDT tokens into nftone marketplace to buy or place buy order
    *
    * @param from
    * @param to
    * @param quant
    * @param memo: $token_id:$order_id:$bid_price
    *       E.g.:  123:1:10288
    */
   void nftone_mart::on_buy_transfer(const name& from, const name& to, const asset& quant, const string& memo) {

      CHECKC( quant.symbol == _gstate.pay_symbol, err::SYMBOL_MISMATCH, "pay symbol not matched")
      if (from == get_self() || to != get_self()) return;

      CHECKC( from != to, err::ACCOUNT_INVALID, "cannot transfer to self" );
      CHECKC( quant.amount > 0, err::PARAM_ERROR, "non-positive quantity not allowed" )
      CHECKC( memo != "", err::MEMO_FORMAT_ERROR, "empty memo!" )

      vector<string_view> params = split(memo, ":");
      auto param_size            = params.size();
      CHECKC( param_size == 3, err::MEMO_FORMAT_ERROR, "memo format incorrect" )

      auto quantity              = quant;
      auto token_id              = to_uint64( params[0], "token_id" );

      auto nstats                = nstats_t::idx_t(NFT_BANK, NFT_BANK.value);
      auto nstats_itr            = nstats.find(token_id);
      CHECKC( nstats_itr         != nstats.end(), err::RECORD_NOT_FOUND, "nft token not found: " + to_string(token_id) )
      auto token_pid             = nstats_itr->supply.symbol.parent_id;
      auto nsymb                 = nsymbol( token_id, token_pid );
      auto orders                = sellorder_idx( _self, token_id );
      auto bought                = nasset(0, nsymb); //by buyer
      asset price                = asset( 0, _gstate.pay_symbol );

      auto bid_price             = price_s(price, nsymb);

      compute_memo_price( string(params[2]), bid_price.value );

      auto order_id              = stoi( string( params[1] ));
      auto itr                   = orders.find( order_id );
      CHECKC( itr != orders.end(), err::RECORD_NOT_FOUND, "order not found: " + to_string(order_id) + "@" + to_string(token_id) )
      CHECKC( quant.amount >= (uint64_t)(bid_price.value.amount), err::PARAM_ERROR, "quantity < price, " + to_string(bid_price.value.amount) )

      uint64_t deal_count        = 0;
      auto order                 = *itr;
      if (order.price <= bid_price) {
         auto count              = bought.amount;
         auto ipowner            = nstats_itr->ipowner;
         auto devfee             = asset(0, _gstate.pay_symbol);
         auto ipfee              = asset(0, _gstate.pay_symbol);
         process_single_buy_order( from, order, quantity, bought, deal_count, devfee, ipowner, ipfee );

         if (order.frozen == 0) {
            orders.erase( itr );

         } else {
            orders.modify(itr, same_payer, [&]( auto& row ) {
               row.frozen = order.frozen;
               row.updated_at = current_time_point();
            });
         }

         deal_trace trace;
         trace.seller_order_id   =  order.id;
         trace.bid_id            =  0;
         trace.maker             =  order.maker;
         trace.buyer             =  from;
         trace.price             =  order.price;
         trace.fee               =  devfee;
         trace.count             =  deal_count;
         trace.ipowner           =  ipowner;
         trace.ipfee             =  ipfee;
         trace.created_at        =  current_time_point();
         _emit_deal_action( trace );

      } else {
         _gstate.last_deal_idx++;
         auto buyerbids          = buyer_bid_t::idx_t(_self, _self.value);
         auto id                 = _gstate.last_deal_idx;
         auto nft_count          = quant.amount / bid_price.value.amount;
         auto frozen             = quant;
         frozen.amount           = nft_count * bid_price.value.amount;
         buyerbids.emplace(_self, [&]( auto& row ){
            row.id               = id;
            row.sell_order_id    = order_id;
            row.price            = bid_price;
            row.frozen           = frozen;
            row.buyer            = from;
            row.created_at       = current_time_point();
         });
         quantity.amount         -= nft_count * bid_price.value.amount;
      }

      if (bought.amount > 0) {
         //send to buyer for nft tokens
         vector<nasset> quants = { bought };
         TRANSFER_N( NFT_BANK, from, quants, "buy nft: " + to_string(token_id) )
      }

      if (quantity.amount > 0) {
         TRANSFER_X( _gstate.bank_contract, from, quantity, "nft buy left" )
      }
   }

   void nftone_mart::_reward_farmer( const asset& fee, const name& farmer ) {
      auto apples = asset(0, APLINK_SYMBOL);
      aplink::farm::available_apples( _gstate.apl_farm.contract, _gstate.apl_farm.lease_id, apples );
      if (apples.amount == 0 || _gstate.apl_farm.unit_reward.amount == 0) return;

      auto reward_amount = wasm::safemath::mul( _gstate.apl_farm.unit_reward.amount, fee.amount, get_precision(APL_SYMBOL) );
      auto reward_quant = asset( reward_amount, APL_SYMBOL );
      ALLOT_APPLE( _gstate.apl_farm.contract, _gstate.apl_farm.lease_id, farmer, reward_quant, "nftone reward" )
   }

   ACTION nftone_mart::takebuybid( const name& seller, const uint32_t& token_id, const uint64_t& buyer_bid_id ) {
      require_auth( seller );

      auto bids                     = buyer_bid_t::idx_t(_self, _self.value);
      auto bid_itr                  = bids.find( buyer_bid_id );
      auto bid_frozen               = bid_itr->frozen;
      auto bid_price                = bid_itr->price;
      auto bid_count                = bid_itr->frozen.amount / bid_itr->price.value.amount;
      CHECKC( bid_itr != bids.end(), err::RECORD_NOT_FOUND, "buyer bid not found: " + to_string( buyer_bid_id ))
      auto sell_order_id = bid_itr->sell_order_id;

      auto sellorders               = sellorder_idx( _self, token_id );
      auto sell_itr                 = sellorders.find( sell_order_id );
      CHECKC( sell_itr != sellorders.end(), err::RECORD_NOT_FOUND, "sell order not found: " + to_string( sell_order_id ))
      CHECKC( seller == sell_itr->maker, err::NO_AUTH, "NO_AUTH")
      auto sell_frozen              = sell_itr->frozen;

      auto nstats                   = nstats_t::idx_t(NFT_BANK, NFT_BANK.value);
      auto nstats_itr               = nstats.find(token_id);
      CHECKC( nstats_itr            != nstats.end(), err::RECORD_NOT_FOUND, "nft token not found: " + to_string(token_id) )
      auto token_pid                = nstats_itr->supply.symbol.parent_id;
      auto nsymb                    = nsymbol( token_id, token_pid );
      auto bought                   = nasset(0, nsymb); //by buyer
      auto earned                   = asset( 0, _gstate.pay_symbol );
      auto fee                      = asset(0, _gstate.pay_symbol);

      auto deal_count      = 0;
      if (bid_count < sell_frozen) {
         deal_count = bid_count;
         bought.amount = bid_count;
         sellorders.modify( sell_itr, same_payer, [&]( auto& row ){
            row.frozen              = sell_frozen - bid_count;
            row.updated_at          = current_time_point();
         });

      } else {
         deal_count = sell_frozen;
         if (bid_count > sell_frozen) {
            auto left = asset( 0, _gstate.pay_symbol );
            left.amount = (bid_count - sell_frozen) * bid_price.value.amount ;
            TRANSFER_X( _gstate.bank_contract, bid_itr->buyer,left , "take nft left" )
         }
         bought.amount = sell_frozen;
         sellorders.erase( sell_itr );
      }

      bids.erase(bid_itr);

      vector<nasset> quants         = { bought };
      TRANSFER_N( NFT_BANK, bid_itr->buyer, quants, "buy nft: " + to_string(token_id) )
      earned.amount                 = bought.amount * bid_price.value.amount;
      
      auto ipowner                  = nstats_itr->ipowner;
      auto devfee                   = asset(0, _gstate.pay_symbol);
      auto ipfee                    = asset(0, _gstate.pay_symbol);
      _settle_maker( bid_itr->buyer, seller, earned, bought, devfee, ipowner, ipfee );

      deal_trace trace;
      trace.seller_order_id         =  sell_itr->id;
      trace.bid_id                  =  bid_itr->id;
      trace.maker                   =  sell_itr->maker;
      trace.buyer                   =  bid_itr->buyer;
      trace.price                   =  bid_price;
      trace.fee                     =  devfee;
      trace.count                   =  deal_count;
      trace.ipowner                 =  ipowner;
      trace.ipfee                   =  ipfee;
      trace.created_at              =  current_time_point();
      _emit_deal_action( trace );

   }


   /////////////////////////////// private funcs below /////////////////////////////////////////////

   void nftone_mart::process_single_buy_order(const name& buyer, order_t& order, asset& quantity, nasset& bought, uint64_t& deal_count, asset& devfee, name& ipowner, asset& ipfee) {
      auto earned                = asset(0, _gstate.pay_symbol); //to seller
      auto offer_cost            = order.frozen * order.price.value.amount;

      if (offer_cost >= quantity.amount) {
         deal_count              = quantity.amount / order.price.value.amount;
         bought.amount           += quantity.amount / order.price.value.amount;
         earned.amount           = bought.amount * order.price.value.amount;
         order.frozen            -= bought.amount;
         quantity.amount         -= earned.amount;

      } else {// will buy the current offer wholely and continue
         deal_count              = order.frozen;
         bought.amount           += order.frozen;
         earned.amount           = offer_cost;
         order.frozen            = 0;
         quantity.amount         -= offer_cost;
      }

      _settle_maker( buyer, order.maker, earned, bought, devfee, ipowner, ipfee);
   }

   void nftone_mart::_settle_maker(const name& buyer, const name& maker, asset& earned, nasset& bought, asset& devfee, const name& ipowner, asset& ipfee) {
      // devfee.amount              =  wasm::safemath::mul(earned.amount, _gstate.dev_fee_rate * 10000, get_precision(earned.symbol));
      // devfee                     /= 10000;
      // ipfee.amount               =  wasm::safemath::mul(earned.amount, _gstate.ipowner_fee_rate * 10000, get_precision(earned.symbol));
      // ipfee                      /= 10000;
      devfee = earned * _gstate.dev_fee_rate;
      ipfee = earned * _gstate.ipowner_fee_rate;

      if (devfee.amount > 0) {
         TRANSFER_X( _gstate.bank_contract, _gstate.dev_fee_collector, devfee, "nftone dev fee" )

         _reward_farmer( devfee, buyer );
      }

      if (ipfee.amount > 0 && ipowner.length() != 0 && is_account(ipowner))
         TRANSFER_X( _gstate.bank_contract, ipowner, ipfee, "nftone ip fee" )

      earned -= devfee + ipfee;
      if (earned.amount > 0)
         TRANSFER_X( _gstate.bank_contract, maker, earned, "sell nft: " + to_string(bought.symbol.id) )

   }

   void nftone_mart::cancelbid( const name& buyer, const uint64_t& buyer_bid_id ){
      require_auth( buyer );
      auto bids                     = buyer_bid_t::idx_t(_self, _self.value);
      auto bid_itr                  = bids.find( buyer_bid_id );
      auto bid_frozen               = bid_itr->frozen;
      auto bid_price                = bid_itr->price;
      CHECKC( bid_itr != bids.end(), err::RECORD_NOT_FOUND, "buyer bid not found: " + to_string( buyer_bid_id ))
      CHECKC( buyer == bid_itr->buyer, err::NO_AUTH, "NO_AUTH")

      auto left = asset( 0, _gstate.pay_symbol );
      TRANSFER_X( _gstate.bank_contract, bid_itr->buyer, bid_frozen, "cancel" )
      bids.erase( bid_itr );
   }

   void nftone_mart::cancelorder(const name& maker, const uint32_t& token_id, const uint64_t& order_id) {
      require_auth( maker );

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

   void nftone_mart::compute_memo_price(const string& memo, asset& price) {
      price.amount =  to_int64( memo, "price");
      CHECKC( price.amount > 0, err::PARAM_ERROR, " price non-positive quantity not allowed" )
   }

   // void nftone_mart::dealtrace(const uint64_t& seller_order_id,
   //                   const uint64_t& bid_id,
   //                   const name& seller,
   //                   const name& buyer,
   //                   const price_s& price,
   //                   const asset& fee,
   //                   const int64_t& count,
   //                   const time_point_sec created_at
   //                 )
   // {
   //    require_auth(get_self());
   //    require_recipient(seller);
   //    require_recipient(buyer);
   // }

   // void nftone_mart::_emit_deal_action(const uint64_t& seller_order_id,
   //                   const uint64_t&   bid_id,
   //                   const name&       maker,
   //                   const name&       buyer,
   //                   const price_s&    price,
   //                   const asset&      fee,
   //                   const int64_t     count,
   //                   const time_point_sec created_at)
   //  {
   //       amax::nftone_mart::deal_trace_action act{ _self, { {_self, active_permission} } };
	// 		act.send( seller_order_id, bid_id, maker, buyer, price, fee, count, created_at );

   // }

   void nftone_mart::dealtrace(const deal_trace& trace) {
      require_auth(get_self());
      require_recipient(trace.maker);
      require_recipient(trace.buyer);
   }

   void nftone_mart::_emit_deal_action(const deal_trace& trace) {
      amax::nftone_mart::deal_trace_action act{ _self, { {_self, active_permission} } };
		act.send( trace );
   }

} //namespace amax