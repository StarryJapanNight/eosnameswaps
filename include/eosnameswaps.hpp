/**
 *  @file
 *  @copyright defined in eos/LICENSE.txt
 */
#pragma once

#include <eosio/eosio.hpp>
#include <eosio/asset.hpp>
#include <eosio/transaction.hpp>
#include <eosio/crypto.hpp>
#include <eosio/time.hpp>

#include "abieos_numeric.hpp"

namespace eosiosystem
{
class system_contract;
}

namespace eosio
{

using std::string;

enum class key_type : uint8_t
{
    k1 = 0,
    r1 = 1,
};

struct key_weight
{
    public_key key;
    uint16_t weight;

    // explicit serialization macro is not necessary, used here only to improve compilation time
    EOSLIB_SERIALIZE(key_weight, (key)(weight))
};

struct permission_level_weight
{
    permission_level permission;
    uint16_t weight;

    // explicit serialization macro is not necessary, used here only to improve compilation time
    EOSLIB_SERIALIZE(permission_level_weight, (permission)(weight))
};

struct wait_weight
{
    uint32_t wait_sec;
    uint16_t weight;

    EOSLIB_SERIALIZE(wait_weight, (wait_sec)(weight))
};

// The definition in native.hpp is different (wrong?)
struct authority
{
    uint32_t threshold;
    std::vector<key_weight> keys;
    std::vector<permission_level_weight> accounts;
    std::vector<wait_weight> waits;

    EOSLIB_SERIALIZE(authority, (threshold)(keys)(accounts)(waits))
};

struct delegated_bandwidth
{
    name from;
    name to;
    asset net_weight;
    asset cpu_weight;

    uint64_t primary_key() const { return to.value; }
};

class[[eosio::contract("eosnameswaps")]] eosnameswaps : public contract
{

public:
    // Transfer memo
    const uint16_t KEY_LENGTH = 53;

    // Bid decision
    const uint16_t BID_REJECTED = 0;
    const uint16_t BID_UNDECIDED = 1;
    const uint16_t BID_ACCEPTED = 2;

    // Constructor
    eosnameswaps(name self, name code, datastream<const char *> ds) : eosio::contract(self, code, ds),
                                                                      _accounts(_self, _self.value),
                                                                      _extras(_self, _self.value),
                                                                      _bids(_self, _self.value),
                                                                      _stats(_self, _self.value),
                                                                      _referrer(_self, _self.value),
                                                                      _shops(_self, _self.value){}
                                                                          // ----------------
                                                                          // Contract Actions
                                                                          // ----------------

                                                                          // Null action
                                                                          [[eosio::action]] void
                                                                          null();

    // Sell account
    [[eosio::action]] void
    sell(name account4sale,
         asset saleprice,
         name paymentaccnt,
         string message);

    // Cancel sale
    [[eosio::action]] void cancel(name account4sale,
                                  string owner_key_str,
                                  string active_key_str);

    // Update the sale price
    [[eosio::action]] void update(name account4sale,
                                  asset saleprice,
                                  string message);

    // Increment the vote count
    [[eosio::action]] void vote(name account4sale,
                                name voter);

    // Register referrer account
    [[eosio::action]] void regref(eosio::name ref_name,
                                  eosio::name ref_account);

    // Register shop
    [[eosio::action]] void regshop(name shopname,
                                   string title,
                                   string description,
                                   name payment1,
                                   name payment2,
                                   name payment3);

    // Propose a bid
    [[eosio::action]] void proposebid(name account4sale,
                                      asset bidprice,
                                      name bidder);

    // Decide on a bid
    [[eosio::action]] void decidebid(name account4sale,
                                     bool accept);

    // Broadcast a message to a user
    [[eosio::action]] void message(name receiver,
                                   string message);

    // Perform screening
    [[eosio::action]] void screener(name account4sale,
                                    uint8_t option);

    // Init the stats table
    [[eosio::action]] void initstats();

    // ------------------
    // Contract Functions
    // ------------------

    // Buy (transfer) action
    void buy(name from,
             name to,
             asset quantity,
             string memo);

    // Buy an account listed for sale
    void buy_saleprice(const name account_to_buy, const name from, const asset quantity, const string owner_key, const string active_key, const string referrer);

    // Buy custom accounts
    void buy_custom(const name account_name, const name from, const asset quantity, const string owner_key, const string active_key);

    // Make a 12 char account
    void make_account(const name account_name, const name from, const asset quantity, const string owner_key, const string active_key);

    // Convert key from string to authority
    authority keystring_authority(string key_str);

    // Update the auth for account4sale
    void account_auth(name account4sale, name changeto, name perm_child, name perm_parent, string pubkey);

    // Send a message action
    void send_message(name to, string message);

private:
    // EOSIO Network (EOS/TELOS)
    //const string symbol_name = "TLOS";
    const string symbol_name = "EOS";

    // Contract network
    symbol network_symbol = symbol(symbol_name, 4);

    // Contract & Referrer fee %
    const float contract_pc = 0.02;
    const float referrer_pc = 0.10;

    // Cost of new account (Feeless)
    const asset newaccountfee = asset(4000, network_symbol);
    const asset newaccountram = asset(2000, network_symbol);
    const asset newaccountcpu = asset(1000, network_symbol);
    const asset newaccountnet = asset(1000, network_symbol);

    // Fees Account
    name feesaccount = name("nameswapsfee");

    // Fund Account
    name nameswapsfnd = name("nameswapsfnd");

    // struct for account table
    struct [[eosio::table]] accounttable
    {
        // Name of account being sold
        name account4sale;

        // Sale price in EOS
        asset saleprice;

        // Account that payment will be sent to
        name paymentaccnt;

        uint64_t primary_key() const { return account4sale.value; }
    };

    eosio::multi_index<name("accounts"), accounttable> _accounts;

    // Struct for extras table
    struct [[eosio::table]] extrastable
    {
        // Name of account being sold
        name account4sale;

        // Has the account been screened for deferred actions?
        bool screened;

        // Number of votes for this name
        uint64_t numberofvotes;

        // Last account to vote for this name
        name last_voter;

        // Message
        string message;

        uint64_t primary_key() const { return account4sale.value; }
    };

    eosio::multi_index<name("extras"), extrastable> _extras;

    // Struct for bids table
    struct [[eosio::table]] bidstable
    {
        // Name of account being sold
        name account4sale;

        // Accepted (2), Undecided (1), Rejected (0)
        uint16_t bidaccepted;

        // The bid price
        asset bidprice;

        // The account making the bid
        name bidder;

        uint64_t primary_key() const { return account4sale.value; }
    };

    eosio::multi_index<name("bids"), bidstable> _bids;

    // Struct for the stats table
    struct [[eosio::table]] statstable
    {
        // Index
        uint64_t index;

        // Number of accounts currently listed
        uint64_t num_listed;

        // Number of accounts purchased
        uint64_t num_purchased;

        // Total sales
        asset tot_sales;

        // Total sales fees
        asset tot_fees;

        uint64_t primary_key() const { return index; }
    };

    eosio::multi_index<name("stats"), statstable> _stats;

    // Struct for the referrer table
    struct [[eosio::table]] reftable
    {
        // Referrer's name
        name ref_name;

        // Referrer's fee account
        name ref_account;

        uint64_t primary_key() const { return ref_name.value; }
    };

    eosio::multi_index<name("referrer"), reftable> _referrer;

    // Struct for the shop table
    struct [[eosio::table]] shopstable
    {
        // Shop name
        name shopname;

        // Shop title
        string title;

        // Shop description
        string description;

        // Payment accounts
        name payment1;
        name payment2;
        name payment3;

        uint64_t primary_key() const { return shopname.value; }
    };

    eosio::multi_index<name("shops"), shopstable> _shops;
};

} // namespace eosio
