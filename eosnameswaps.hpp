/**
 *  @file
 *  @copyright defined in eos/LICENSE.txt
 */
#pragma once

#include <eosiolib/asset.hpp>
#include <eosiolib/currency.hpp>
#include <eosiolib/eosio.hpp>
#include <eosio.system/native.hpp>
#include <eosiolib/public_key.hpp>
#include "includes/eosio.token.hpp"
#include "includes/abieos_numeric.hpp"

namespace eosiosystem
{
class system_contract;
}

namespace eosio
{

using std::string;

struct wait_weight
{
    uint32_t wait_sec;
    weight_type weight;

    EOSLIB_SERIALIZE(wait_weight, (wait_sec)(weight))
};

// The definition in native.hpp is different (wrong?)
struct authority
{
    uint32_t threshold;
    vector<eosiosystem::key_weight> keys;
    vector<eosiosystem::permission_level_weight> accounts;
    vector<wait_weight> waits;

    EOSLIB_SERIALIZE(authority, (threshold)(keys)(accounts)(waits))
};

struct sell_type
{
    account_name account4sale;
    asset saleprice;
    account_name paymentaccnt;
};

struct cancel_type
{
    account_name account4sale;
};

struct user_resources
{
    account_name owner;
    asset net_weight;
    asset cpu_weight;
    int64_t ram_bytes = 0;

    uint64_t primary_key() const { return owner; }

};

struct token_account
{
    asset balance;

    uint64_t primary_key() const { return balance.symbol.name(); }
};

class eosnameswaps : public contract
{

  public:
    // Constructor
    eosnameswaps(account_name self) : contract(self), _accounts(self, self) {}

    // Buy (transfer) action
    void buy(const currency::transfer &transfer_data);

    // Sell account action
    void sell(const sell_type &sell_data);

    // Cancel sale action
    void cancel(const cancel_type &cancel_data);

    // Update the auth for account4sale
    void account_auth(account_name account4sale, account_name changeto, permission_name perm_child, permission_name perm_parent);

    // Apply (main) function
    void apply(const account_name contract, const account_name act);

    auto get_salefee()
    {
        return salefee;
    }

    auto get_contractfees()
    {
        return contractfees;
    }

  private:
    // % fee taken by the contract for sale
    const double salefee = 0.05;

    // Account to transfer fees to
    const account_name contractfees = N(namedaccount);

    // Account struct for table
    //@abi table accounts
    struct account
    {
        // Name of account being sold
        account_name account4sale;

        // Sale price in EOS
        asset saleprice;

        // Account that payment will be sent to
        account_name paymentaccnt;

        uint64_t primary_key() const { return account4sale; }
    };

    eosio::multi_index<N(accounts), account> _accounts;
};

} // namespace eosio
