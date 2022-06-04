#include <nftone.mart/nftone.mart.hpp>
#include <amax.ntoken/amax.ntoken_db.hpp>

namespace amax {

using namespace std;

#define CHECKC(exp, code, msg) \
   { if (!(exp)) eosio::check(false, string("$$$") + to_string((int)code) + string("$$$ ") + msg); }


   /**
    * @brief send nasset tokens into nftone marketplace
    *
    * @param from
    * @param to
    * @param quantity
    * @param memo
    */
   [[eosio::on_notify("amax.ntoken::transfer")]]
   void nftone_mart::ontransfer(const name& from, const name& to, const nasset& quantity, const string& memo) {
      CHECKC( from != to, err::ACCOUNT_INVALID,"cannot transfer to self" );
      CHECKC( quantity.amount > 0, err::PARAM_ERROR, "non-positive quantity not allowed" )
      CHECKC( memo != "", err::MEMO_FORMAT_ERROR, "empty memo!" )

      auto offers = offer_t::idx_t( _self, _self.value );
      auto itr = offers.find( quantity.symbol.raw() );
      if (itr == offers.end()) {
         offers.emplace( _self, [&]( auto& row ) {
            row.id            = offers.available_primary_key(); if (row.id == 0) row.id = 1;
            row.owner         = from;
            row.quantity      = quantity;
            row.created_at    = time_point_sec( current_time_point() );
         });
      } else {
         accounts.modify(itr, same_payer, [&]( auto& row ) {
            row.quantity      += quantity;
         });
      }
   }

   void nftone_mart::onsale(const name& owner, const nsymbol& nft_symb, asset& cnyd_price, const uint64_t& start_in_seconds) {
      require_auth( owner );

      CHECKC( cnyd_price.symbol == CNYD_SYMB, err::PARAM_ERROR, "price symbol is not CNYD" )
      CHECKC( cnyd_price.quantity.amount > 0, err::PARAM_ERROR, "asset price must be positive" )

      auto offers = offers_t::idx_t( _self, _self.value );
      auto idx = offers.get_index<"ownernfts"_n>();
      auto sk = (uint128_t) owner.value | nft_symb.raw();
      auto itr = idx.find(sk);
      CHECKC( itr != idx.end(), err::RECORD_NOT_FOUND, "nft not found: " + nft_symb.to_string() )
      
      idx.modify( itr, same_payer,[&]( auto& row ) {
         row.price = cnyd_price;
      });
   }

} //namespace amax