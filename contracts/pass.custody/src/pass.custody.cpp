
#include <amax.ntoken/amax.ntoken.hpp>
#include "pass.custody.hpp"
#include "commons/utils.hpp"

#include <chrono>

using std::chrono::system_clock;
using namespace wasm;
using namespace amax;

static constexpr eosio::name active_permission{"active"_n};

// transfer out from contract self
#define TRANSFER_OUT(token_contract, to, quantity, memo) ntoken::transfer_action(                                \
                                                             token_contract, {{get_self(), active_permission}}) \
                                                             .send( get_self(), to, quantity, memo );

[[eosio::action]]
void custody::init() {
    require_auth( _self );

    // auto issues = lock_t::tbl_t(_self, _self.value);
    // auto itr = issues.begin();
    // int step = 0;
    // while (itr != issues.end()) {
    //     if (step > 30) return;

    //     if (itr->plan_id != 1 || itr->issuer != "armoniaadmin"_n) {
    //         itr = issues.erase( itr );
    //         step++;
    //     } else 
    //         itr++;
    // }

    // auto plans = plan_t::tbl_t(_self, _self.value);
    // auto itr = plans.begin();
    // int step = 0;
    // while (itr != plans.end()) {
    //     if (step > 30) return;

    //     if (itr->id != 1) {
    //         itr = plans.erase( itr );
    //         step++;
    //     } else 
    //         itr++;
    // }

    CHECKC( false, err::MISC, "n/a" )
}

[[eosio::action]]
void custody::setconfig(const asset &plan_fee, const name &fee_receiver) {
    require_auth(get_self());

    CHECK(plan_fee.symbol == SYS_SYMBOL, "plan_fee symbol mismatch with sys symbol")
    CHECK(plan_fee.amount >= 0, "plan_fee symbol amount can not be negative")
    CHECK(is_account(fee_receiver), "fee_receiver account does not exist")

    _gstate.plan_fee = plan_fee;
    _gstate.fee_receiver = fee_receiver;
    _global.set( _gstate, get_self() );
}

//add a lock plan
[[eosio::action]] void custody::addplan(const name& owner,
                                        const string& title, const name& asset_contract, const nsymbol& asset_symbol,
                                        const uint64_t& unlock_interval_days, const int64_t& unlock_times)
{
    require_auth(owner);

    CHECK( title.size() <= MAX_TITLE_SIZE, "title size must be <= " + to_string(MAX_TITLE_SIZE) )
    CHECK( is_account(asset_contract), "asset contract account does not exist" )
    CHECK( asset_symbol.is_valid(), "Invalid asset symbol" )
    CHECK( unlock_interval_days > 0 && unlock_interval_days <= MAX_LOCK_DAYS, "unlock_days must be > 0 and <= 365*10, i.e. 10 years" )
    CHECK( unlock_times > 0, "unlock times must be > 0" )
    if (_gstate.plan_fee.amount > 0) {
        CHECK(_gstate.fee_receiver.value != 0, "fee_receiver not set")
    }
    plan_t::tbl_t plan_tbl(get_self(), get_self().value);

    plan_tbl.emplace( owner, [&]( auto& plan ) {
        plan.id                     = ++_gstate.last_plan_id;
        plan.owner                  = owner;
        plan.title                  = title;
        plan.asset_contract         = asset_contract;
        plan.asset_symbol           = asset_symbol;
        plan.unlock_interval_days   = unlock_interval_days;
        plan.unlock_times           = unlock_times;
        plan.total_issued           = nasset(0, asset_symbol);
        plan.total_unlocked         = nasset(0, asset_symbol);
        plan.total_refunded         = nasset(0, asset_symbol);
        plan.status                 =  _gstate.plan_fee.amount != 0 ? plan_status::feeunpaid : plan_status::enabled;
        plan.created_at             = current_time_point();
        plan.updated_at             = plan.created_at;
    });

    account::tbl_t pay_account_tbl(get_self(), get_self().value);
    pay_account_tbl.set(owner.value, owner, [&]( auto& acct ) {
        acct.owner                  = owner;
        acct.last_plan_id           = _gstate.last_plan_id;
    });
}

[[eosio::action]]
void custody::setplanowner(const name& owner, const uint64_t& plan_id, const name& new_owner) {
    plan_t::tbl_t plan_tbl(get_self(), get_self().value);
    auto plan_itr = plan_tbl.find(plan_id);
    CHECK( plan_itr != plan_tbl.end(), "plan not found: " + to_string(plan_id) )
    CHECK( owner == plan_itr->owner, "owner mismatch" )
    CHECK( has_auth(plan_itr->owner) || has_auth(get_self()), "Missing required authority of owner or maintainer" )
    CHECK( is_account(new_owner), "new_owner account does not exist");

    plan_tbl.modify( plan_itr, same_payer, [&]( auto& plan ) {
        plan.owner = new_owner;
        plan.updated_at = current_time_point();
    });
}

// [[eosio::action]]
// void custody::delplan(const name& owner, const uint64_t& plan_id) {
//     require_auth(get_self());

//     plan_t::tbl_t plan_tbl(get_self(), get_self().value);
//     auto plan_itr = plan_tbl.find(plan_id);
//     CHECK( plan_itr != plan_tbl.end(), "plan not found: " + to_string(plan_id) )
//     CHECK( owner == plan_itr->owner, "owner mismatch" )

//     plan_tbl.erase(plan_itr);
// }

// [[eosio::action]]
// void custody::enableplan(const name& owner, const uint64_t& plan_id, bool enabled) {
//     require_auth(owner);

//     plan_t::tbl_t plan_tbl(get_self(), get_self().value);
//     auto plan_itr = plan_tbl.find(plan_id);
//     CHECK( plan_itr != plan_tbl.end(), "plan not found: " + to_string(plan_id) )
//     CHECK( owner == plan_itr->owner, "owner mismatch" )
//     CHECK( plan_itr->status != PLAN_UNPAID_FEE, "plan is unpaid fee status" )
//     plan_status_t new_status = enabled ? PLAN_ENABLED : PLAN_DISABLED;
//     CHECK( plan_itr->status != new_status, "plan status is no changed" )

//     plan_tbl.modify( plan_itr, same_payer, [&]( auto& plan ) {
//         plan.status = new_status;
//         plan.updated_at = current_time_point();
//     });
// }

//issue-in op: transfer tokens to the contract and lock them according to the given plan
[[eosio::action]]
void custody::ontransfer(name from, name to, nasset quantity, string memo) {
    if (from == get_self() || to != get_self()) return;

	CHECK( quantity.amount > 0, "quantity must be positive" )

    //memo params format:
    //1. plan:${plan_id}, Eg: "plan:" or "plan:1"
    //2. issue:${receiver}:${plan_id}:${first_unlock_days}, Eg: "issue:receiver1234:1:30"
    vector<string_view> memo_params = split(memo, ":");
    ASSERT(memo_params.size() > 0);
    if (memo_params[0] == "plan") {
        CHECK(memo_params.size() == 2, "ontransfer:plan params size of must be 2")
        auto param_plan_id = memo_params[1];

        CHECK( quantity.amount == _gstate.plan_fee.amount,
            "quantity amount mismatch with fee amount: " + to_string(_gstate.plan_fee.amount) );
        uint64_t plan_id = 0;
        if (param_plan_id.empty()) {
            account::tbl_t pay_account_tbl(get_self(), get_self().value);
            auto acct = pay_account_tbl.get(from.value, "from account does not exist in custody constract");
            plan_id = acct.last_plan_id;
            CHECK( plan_id != 0, "from account does no have any plan" );
        } else {
            plan_id = to_uint64(param_plan_id.data(), "plan_id");
            CHECK( plan_id != 0, "plan id can not be 0" );
        }

        plan_t::tbl_t plan_tbl(get_self(), get_self().value);
        auto plan_itr           = plan_tbl.find(plan_id);
        CHECK( plan_itr != plan_tbl.end(), "plan not found by plan_id: " + to_string(plan_id) )
        CHECK( plan_itr->status == plan_status::feeunpaid, "plan must be unpaid fee, status:" + plan_itr->status.to_string() )
        plan_tbl.modify( plan_itr, same_payer, [&]( auto& plan ) {
            plan.status         = plan_status::enabled;
            plan.updated_at     = current_time_point();
        });

        TRANSFER_OUT( get_first_receiver(), _gstate.fee_receiver, quantity, memo )

    } else if (memo_params[0] == "issue") {
        CHECK(memo_params.size() == 4, "ontransfer:issue params size of must be 4")
        auto receiver           = name(memo_params[1]);
        auto plan_id            = to_uint64(memo_params[2], "plan_id");
        auto first_unlock_days  = to_uint64(memo_params[3], "first_unlock_days");

        plan_t::tbl_t plan_tbl(get_self(), get_self().value);
        auto plan_itr           = plan_tbl.find(plan_id);
        CHECK( plan_itr != plan_tbl.end(), "plan not found: " + to_string(plan_id) )
        CHECK( plan_itr->status == plan_status::enabled, "plan not enabled, status:" + plan_itr->status.to_string() )

        CHECK( is_account(receiver), "receiver account not exist" );
        CHECK( first_unlock_days <= MAX_LOCK_DAYS, "unlock_days must be > 0 and <= 365*10, i.e. 10 years" )
        CHECK( quantity.symbol == plan_itr->asset_symbol, "symbol of quantity mismatch with symbol of plan" );
        CHECK( quantity.amount > 0, "quantity must be positive" )
        CHECK( plan_itr->asset_contract == get_first_receiver(), "issue asset contract mismatch" );
        CHECK( plan_itr->asset_symbol == quantity.symbol, "issue asset symbol mismatch" );

        auto now = current_time_point();
        plan_tbl.modify( plan_itr, same_payer, [&]( auto& plan ) {
            plan.total_issued       += quantity;
            plan.last_lock_id       += 1;
            plan.updated_at         = now;
        });

        lock_t::tbl_t lock_tbl(get_self(), plan_id);
        lock_tbl.emplace( _self, [&]( auto& lock ) {
            lock.id                 = plan_itr->last_lock_id;
            lock.locker             = from;
            lock.receiver           = receiver;
            lock.first_unlock_days  = first_unlock_days;
            lock.issued             = quantity;
            lock.locked             = quantity;
            lock.unlocked           = nasset(0, quantity.symbol);
            lock.unlock_interval_days = plan_itr->unlock_interval_days;
            lock.unlock_times       = plan_itr->unlock_times;
            lock.status             = lock_status::locked;
            lock.locked_at          = now;
            lock.updated_at         = now;
        });

    }
    // else { ignore }
}

[[eosio::action]]
void custody::endissue(const name& issuer, const uint64_t& plan_id, const uint64_t& issue_id) {
    CHECK( has_auth( issuer ) || has_auth( _self ), "not authorized to end issue" )
    // require_auth( issuer );

    _unlock(issuer, plan_id, issue_id, /*is_end_action=*/true);
}

/**
 * withraw all available/unlocked assets belonging to the issuer
 */
[[eosio::action]]
void custody::unlock(const name& receiver, const uint64_t& plan_id, const uint64_t& issue_id) {
    require_auth(receiver);

    _unlock(receiver, plan_id, issue_id, /*is_end_action=*/false);
}

void custody::_unlock(const name& issuer, const uint64_t& plan_id, const uint64_t& issue_id, bool to_terminate)
{
    auto now = current_time_point();

    lock_t::tbl_t lock_tbl(get_self(), plan_id);
    auto lock_itr = lock_tbl.find(issue_id);
    CHECK( lock_itr != lock_tbl.end(), "issue not found: " + to_string(issue_id) )

    plan_t::tbl_t plan_tbl(get_self(), get_self().value);
    auto plan_itr = plan_tbl.find(plan_id);
    CHECK( plan_itr != plan_tbl.end(), "plan not found: " + to_string(plan_id) )
    CHECK( plan_itr->status == plan_status::enabled, "plan not enabled, status:" + plan_itr->status.to_string() )

    if (to_terminate) {
        CHECK( lock_itr->status != lock_status::unlocked, "issue has been ended, status: " + lock_itr->status.to_string() );

        CHECK( issuer == lock_itr->locker || issuer == _self, "not authorized" )

    } else {
        CHECK( lock_itr->status == lock_status::locked, "issue not normal, status: " + lock_itr->status.to_string() );
    }

    int64_t total_unlocked = 0;
    int64_t remaining_locked = lock_itr->locked.amount;
    if (lock_itr->status == lock_status::locked) {
        ASSERT(now >= lock_itr->locked_at);
        
        auto issued_days = (now.sec_since_epoch() - lock_itr->locked_at.sec_since_epoch()) / DAY_SECONDS;
        auto unlocked_days = issued_days > lock_itr->first_unlock_days ? issued_days - lock_itr->first_unlock_days : 0;
        ASSERT(plan_itr->unlock_interval_days > 0);
        auto unlocked_times = std::min(unlocked_days / plan_itr->unlock_interval_days, plan_itr->unlock_times);
        if (unlocked_times >= plan_itr->unlock_times) {
            total_unlocked = lock_itr->issued.amount;
        } else {
            ASSERT(plan_itr->unlock_times > 0);
            total_unlocked = multiply_decimal64(lock_itr->issued.amount, unlocked_times, plan_itr->unlock_times);
            ASSERT(total_unlocked >= lock_itr->unlocked.amount && lock_itr->issued.amount >= total_unlocked);
        }

        int64_t cur_unlocked = total_unlocked - lock_itr->unlocked.amount;
        remaining_locked = lock_itr->issued.amount - total_unlocked;
        ASSERT(remaining_locked >= 0);

        TRACE("unlock detail: ", PP0(issued_days), PP(unlocked_days), PP(unlocked_times), PP(total_unlocked),
            PP(cur_unlocked), PP(remaining_locked), "\n");

        if (cur_unlocked > 0) {
            auto unlock_quantity    = nasset(cur_unlocked, plan_itr->asset_symbol);
            string memo             = "unlock: " + to_string(issue_id) + "@" + to_string(plan_id);
            TRANSFER_OUT( plan_itr->asset_contract, lock_itr->receiver, unlock_quantity, memo )

        } else { // cur_unlocked == 0
            if (!to_terminate) {
                CHECK( false, "It's not time to unlock yet" )
            } // else ignore
        }

        uint64_t refunded = 0;
        if (to_terminate && remaining_locked > 0) {
            refunded                = remaining_locked;
            auto memo               = "refund: " + to_string(issue_id);
            auto refunded_quantity  = nasset(refunded, plan_itr->asset_symbol);
            TRANSFER_OUT( plan_itr->asset_contract, lock_itr->locker, refunded_quantity, memo )
            remaining_locked        = 0;
        }

        plan_tbl.modify( plan_itr, same_payer, [&]( auto& plan ) {
            plan.total_unlocked.amount += cur_unlocked;
            if (refunded > 0) {
                plan.total_refunded.amount += refunded;
            }
            plan.updated_at = current_time_point();
        });
    }

    lock_tbl.modify( lock_itr, same_payer, [&]( auto& lock ) {
        lock.unlocked.amount = total_unlocked;
        lock.locked.amount = remaining_locked;
        if (to_terminate || lock.unlocked == lock.issued) {
            lock.status = lock_status::unlocked;
        }
        lock.updated_at = current_time_point();
    });
}

