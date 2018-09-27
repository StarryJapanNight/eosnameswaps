/**
 *  @file
 *  @copyright defined in eos/LICENSE.txt
 */
#pragma once

#include <eosiolib/asset.hpp>
#include <eosiolib/currency.hpp>
#include <eosio.system/native.hpp>
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
    string message;
};

struct cancel_type
{
    account_name account4sale;
    string owner_key_str;
    string active_key_str;
};

struct updatesale_type
{
    account_name account4sale;
    asset saleprice;
    string message;
};

struct likes_type
{
    account_name account4sale;
    account_name liker;
};

struct admin_type
{
    account_name account4sale;
    string action;
    string option;
};

struct user_resources
{
    account_name owner;
    asset net_weight;
    asset cpu_weight;
    int64_t ram_bytes = 0;

    uint64_t primary_key() const { return owner; }
};

class eosnameswaps : public contract
{

  public:
    // Constructor
    eosnameswaps(account_name self) : contract(self), _accounts(self, self), _extras(self, self) {}

    // Buy (transfer) action
    void buy(const currency::transfer &transfer_data);

    // Sell account action
    void sell(const sell_type &sell_data);

    // Cancel sale action
    void cancel(const cancel_type &cancel_data);

    // Update the sale price
    void updatesale(const updatesale_type &updatesale_data);

    // Increment the like count
    void likes(const likes_type &likes_data);

    // Perform admin tasks
    void admin(const admin_type &admin_data);

    // Update the auth for account4sale
    void account_auth(account_name account4sale, account_name changeto, permission_name perm_child, permission_name perm_parent, string pubkey);

    // Apply (main) function
    void apply(const account_name contract, const account_name act);

    // Return the contract fees percentage
    auto get_salefee()
    {
        return salefee;
    }

    // Return the contract fees account
    auto get_contractfees()
    {
        return contractfees;
    }

  private:
    // % fee taken by the contract for sale
    const double salefee = 0.02;

    // Account to transfer fees to
    const account_name contractfees = N(namedaccount);

    // struct for account table
    struct account_table
    {
        // Name of account being sold
        account_name account4sale;

        // Sale price in EOS
        asset saleprice;

        // Account that payment will be sent to
        account_name paymentaccnt;

        uint64_t primary_key() const { return account4sale; }
    };

    eosio::multi_index<N(accounts), account_table> _accounts;

    // Struct for extras table
    struct extras_table
    {
        // Name of account being sold
        account_name account4sale;

        // Has the account been screened for deferred actions?
        bool screened;

        // Number of likes for this name
        uint64_t numberoflikes;

        // Last account to like this name
        account_name last_liked;

        // Message
        string message;

        uint64_t primary_key() const { return account4sale; }
    };

    eosio::multi_index<N(extras), extras_table> _extras;
};

} // namespace eosio
