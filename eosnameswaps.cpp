/**
             *  @file
             *  @copyright defined in eos/LICENSE.txt
             */

#include "eosnameswaps.hpp"

namespace eosio
{

// Action: Sell an account
void eosnameswaps::sell(const sell_type &sell_data)
{

    // Check an account with that name is not already listed for sale
    auto itr = _accounts.find(sell_data.account4sale);
    eosio_assert(itr == _accounts.end(), "That account is already for sale.");

    // Basic checks (redundant?)
    eosio_assert(sell_data.account4sale != N(), "Sell Error: You must specify an account name to cancel.");
    eosio_assert(sell_data.account4sale != _self, "Sell Error: you cannot buy/sell/cancel the contract!.");

    // Check the payment account exists
    eosio_assert(is_account(sell_data.paymentaccnt), "Sell Error: The Payment account does not exist.");

    // Check the transfer is valid
    eosio_assert(sell_data.saleprice.symbol == string_to_symbol(4, "EOS"), "Sell Error: Sale price must be in EOS. Ex: '10.0000 EOS'.");
    eosio_assert(sell_data.saleprice.is_valid(), "Sell Error: Price is not valid.");
    eosio_assert(sell_data.saleprice >= asset(10000), "Sell Error: Sale price must be at least 1 EOS. Ex: '1.0000 EOS'.");

    // Only the account4sale@owner can sell (contract@eosio.code must already be an owner)
    require_auth2(sell_data.account4sale, N(owner));

    // ----------------------------------------------
    // Staking checks
    // ----------------------------------------------

    // Lookup the userres table stored on eosio
    typedef eosio::multi_index<N(userres), user_resources> user_resources_table;
    user_resources_table userres_table(N(eosio), sell_data.account4sale);
    auto itr_usr = userres_table.find(sell_data.account4sale);

    //  Assertion checks
    eosio_assert(itr_usr->cpu_weight.amount / 10000.0 >= 0.5, "You can not sell an account with less than 0.5 EOS staked to CPU. Contract actions will fail with less.");
    eosio_assert(itr_usr->net_weight.amount / 10000.0 >= 0.1, "You can not sell an account with less than 0.1 EOS staked to NET. Contract actions will fail with less.");

    // ----------------------------------------------
    // Change account ownership
    // ----------------------------------------------

    // Change auth from account4sale@active to contract@active
    // This ensures eosio.code permission has been set to the contract
    account_auth(sell_data.account4sale, _self, N(active), N(owner), "None");

    // Change auth from contract@owner to owner@owner
    // This ensures the contract is the only owner
    account_auth(sell_data.account4sale, _self, N(owner), N(), "None");

    // Place data in table. Seller pays for ram storage
    _accounts.emplace(sell_data.account4sale, [&](auto &s) {
        s.account4sale = sell_data.account4sale;
        s.saleprice = sell_data.saleprice;
        s.paymentaccnt = sell_data.paymentaccnt;
    });
}

// Action: Buy an account listed for sale
void eosnameswaps::buy(const currency::transfer &transfer_data)
{

    // Ignore transfers from the contract or our funding account.
    // This is necessary to allow transfers of EOS to/from the contract.
    if (transfer_data.from == _self || transfer_data.from == N(namedaccount))
    {
        return;
    }

    // ----------------------------------------------
    // Valid transaction checks
    // ----------------------------------------------

    // Check the transfer is valid
    eosio_assert(transfer_data.quantity.symbol == string_to_symbol(4, "EOS"), "Buy Error: You must pay in EOS.");
    eosio_assert(transfer_data.quantity.is_valid(), "Buy Error: Quantity is not valid.");
    eosio_assert(transfer_data.quantity >= asset(10000), "Buy Error: Minimum price is 1.0000 EOS.");

    // Find the length of the account name
    int name_length;
    for (int lp = 1; lp <= 12; ++lp)
    {

        if (transfer_data.memo[lp] == ',')
        {
            name_length = lp + 1;
        }
    }

    // Extract account to buy from memo
    const string account_string = transfer_data.memo.substr(0, name_length);
    const account_name account_to_buy = string_to_name(account_string.c_str());

    const string owner_key_str = transfer_data.memo.substr(name_length, 53);
    string active_key_str;

    if (transfer_data.memo[name_length + 54] == ',')
    {
        // An active key has been provided
        active_key_str = transfer_data.memo.substr(name_length + 55, 53);
    }
    else
    {
        // The active key is the same as owner
        active_key_str = owner_key_str;
    }

    // Check an account name has been listed in the memo
    eosio_assert(account_to_buy != N(), "Buy Error: You must specify an account name to buy in the memo.");
    eosio_assert(account_to_buy != _self, "Buy Error: You cannot buy/sell/cancel the contract!.");

    // Check the account is available to buy
    auto accounts_itr = _accounts.find(account_to_buy);
    eosio_assert(accounts_itr != _accounts.end(), std::string("Buy Error: Account " + name{account_to_buy}.to_string() + " is not for sale.").c_str());

    // Check the correct amount of EOS was transferred
    eosio_assert(accounts_itr->saleprice.amount == transfer_data.quantity.amount, "Buy Error: You have not transferred the correct amount of EOS. Check the sale price.");

    // Check the account has been screened for inflight msig/deferred transactions, and is available for sale.
    auto isforsale_itr = _isforsale.find(account_to_buy);
    eosio_assert(isforsale_itr != _isforsale.end(), "Buy Error: The account you wish to buy has not yet passed screening. Please wait a few hours.");

    // ----------------------------------------------
    // Seller and contract fees
    // ----------------------------------------------

    // Sale price
    auto saleprice = accounts_itr->saleprice;

    // Contract fee
    auto contractfee = saleprice;
    contractfee.amount = get_salefee() * saleprice.amount;

    // Seller fee
    auto sellerfee = saleprice;
    sellerfee.amount = saleprice.amount - contractfee.amount;

    // Transfer EOS from contract to contract fees account
    action(
        permission_level{_self, N(owner)},
        N(eosio.token), N(transfer),
        std::make_tuple(_self, get_contractfees(), contractfee, std::string("EOSNAMESWAPS: Account sale contract fee: " + name{accounts_itr->account4sale}.to_string())))
        .send();

    // Transfer EOS from contract to seller minus the contract fee
    action(
        permission_level{_self, N(owner)},
        N(eosio.token), N(transfer),
        std::make_tuple(_self, accounts_itr->paymentaccnt, sellerfee, std::string("EOSNAMESWAPS: Account sale fee: " + name{accounts_itr->account4sale}.to_string())))
        .send();

    // ----------------------------------------------
    // Update account owner
    // ----------------------------------------------

    // Remove contract@owner permissions and replace with buyer@active account and the supplied key
    account_auth(accounts_itr->account4sale, transfer_data.from, N(active), N(owner), active_key_str);

    // Remove seller@active permissions and replace with buyer@owner account and the supplie key
    account_auth(accounts_itr->account4sale, transfer_data.from, N(owner), N(), owner_key_str);

    // Erase account from accounts table
    _accounts.erase(accounts_itr);

    // Erase account from isforsale table
    _isforsale.erase(isforsale_itr);
}

// Action: Remove a listed account from sale
void eosnameswaps::cancel(const cancel_type &cancel_data)
{

    // ----------------------------------------------
    // Cancel data checks
    // ----------------------------------------------

    // Check an account with that name is listed for sale
    auto accounts_itr = _accounts.find(cancel_data.account4sale);
    eosio_assert(accounts_itr != _accounts.end(), "Cancel Error: That account name is not listed for sale");

    // Only the paymentaccnt can cancel the sale (the contract has the owner key)
    eosio_assert(has_auth(N(_self)) || has_auth(accounts_itr->paymentaccnt), "Cancel Error: Only the payment account can cancel the sale.");

    // Basic checks (redundant?)
    eosio_assert(cancel_data.account4sale != N(), "Cancel Error: You must specify an account name to cancel.");
    eosio_assert(cancel_data.account4sale != _self, "Cancel Error: You cannot buy/sell/cancel the contract!.");

    // ----------------------------------------------
    // Update account owners
    // ----------------------------------------------

    // Change auth from contract@active to submitted active key
    account_auth(cancel_data.account4sale, accounts_itr->paymentaccnt, N(active), N(owner), cancel_data.active_key_str);

    // Change auth from contract@owner to submitted owner key
    account_auth(cancel_data.account4sale, accounts_itr->paymentaccnt, N(owner), N(), cancel_data.owner_key_str);

    // ----------------------------------------------
    // Cleanup
    // ----------------------------------------------

    // Erase account from accounts table
    _accounts.erase(accounts_itr);

    // Remove account4sale from isforsale table if necessary
    auto isforsale_itr = _isforsale.find(cancel_data.account4sale);
    if (isforsale_itr != _isforsale.end())
    {
        // Erase account from isforsale table
        _isforsale.erase(isforsale_itr);
    }
}

// Action: Add/Remove a listed account from sale
void eosnameswaps::updatesale(const updatesale_type &updatesale_data)
{
    // Only the developer can set an account for sale after screening for inflight msig/deferred transactions.
    // This is necessary to secure against a vunerability whereby an account can be taken back after being sold.
    require_auth(_self);

    // Find the account in the isforsale  and accounts tables
    auto isforsale_itr = _isforsale.find(updatesale_data.account4sale);
    auto accounts_itr = _accounts.find(updatesale_data.account4sale);

    // Add account4sale to the OK to sell list
    if (updatesale_data.addremove == true)
    {

        // Check the account has been listed for sale by the owner
        eosio_assert(accounts_itr != _accounts.end(), "Developer Error: You can't approve an account that is yet to be sold!");

        // Check the account4sale isn't already listed
        eosio_assert(isforsale_itr == _isforsale.end(), "Developer Error: That account is already in the table!");

        // Place data in table. Contract pays for ram storage
        _isforsale.emplace(_self, [&](auto &s) {
            s.account4sale = updatesale_data.account4sale;
        });
    }

    // Remove account4sale from the OK to sell list
    if (updatesale_data.addremove == false)
    {

        // Check the account4sale is already listed
        eosio_assert(isforsale_itr != _isforsale.end(), "Developer Error: That account is not in the table!");

        // Erase account from isforsale table
        _isforsale.erase(isforsale_itr);
    }
}

void eosnameswaps::account_auth(account_name account4sale, account_name changeto, permission_name perm_child, permission_name perm_parent, string pubkey_str)
{

    // Setup authority for contract. Choose either a new key, or account, or both.
    authority contract_authority;
    if (pubkey_str != "None")
    {
        // Convert the public key string to the requried type
        const abieos::public_key pubkey = abieos::string_to_public_key(pubkey_str);

        // Array to hold public key
        array<char, 33> pubkey_char;
        std::copy(pubkey.data.begin(), pubkey.data.end(), pubkey_char.begin());

        eosiosystem::key_weight kweight{
            .key = {(uint8_t)abieos::key_type::k1, pubkey_char},
            .weight = (uint16_t)1};

        // Key is supplied
        contract_authority.threshold = 1;
        contract_authority.keys = {kweight};
        contract_authority.accounts = {};
        contract_authority.waits = {};
    }
    else
    {

        // Account to take over permission changeto@perm_child
        eosiosystem::permission_level_weight accounts{
            .permission = permission_level{changeto, perm_child},
            .weight = (uint16_t)1};

        // Key is not supplied
        contract_authority.threshold = 1;
        contract_authority.keys = {};
        contract_authority.accounts = {accounts};
        contract_authority.waits = {};
    }

    // Remove contract permissions and replace with changeto account.
    action(permission_level{account4sale, N(owner)},
           N(eosio), N(updateauth),
           std::make_tuple(
               account4sale,
               perm_child,
               perm_parent,
               contract_authority))
        .send();
} // namespace eosio

void eosnameswaps::reauth(const reauth_type &reauth_data)
{

    // Only the contract can update an accounts auth
    require_auth(_self);

    // Change auth from account4sale@active to contract@active
    // This ensures eosio.code permission has been set to the contract
    account_auth(reauth_data.account4sale, _self, N(active), N(owner), "None");

    // Change auth from contract@owner to owner@owner
    // This ensures the contract is the only owner
    account_auth(reauth_data.account4sale, _self, N(owner), N(), "None");
}

void eosnameswaps::apply(const account_name contract, const account_name act)
{

    switch (act)
    {
    case N(transfer):

        if (contract == N(eosio.token))
        {
            buy(unpack_action_data<currency::transfer>());
        }
        break;
    case N(sell):
        sell(unpack_action_data<sell_type>());
        break;
    case N(cancel):
        cancel(unpack_action_data<cancel_type>());
        break;
    case N(updatesale):
        updatesale(unpack_action_data<updatesale_type>());
        break;
    case N(reauth):
        reauth(unpack_action_data<reauth_type>());
        break;
    default:
        break;
    }
}

extern "C"
{
    [[noreturn]] void apply(uint64_t receiver, uint64_t code, uint64_t action) {
        eosnameswaps _eosnameswaps(receiver);

        switch (action)
        {
        case N(transfer):
            if (code == N(eosio.token))
            {
                _eosnameswaps.buy(unpack_action_data<currency::transfer>());
            }
            break;
        case N(sell):
            _eosnameswaps.sell(unpack_action_data<sell_type>());
            break;
        case N(cancel):
            _eosnameswaps.cancel(unpack_action_data<cancel_type>());
            break;
        case N(updatesale):
            _eosnameswaps.updatesale(unpack_action_data<updatesale_type>());
            break;
        case N(reauth):
            _eosnameswaps.reauth(unpack_action_data<reauth_type>());
            break;
        default:
            break;
        }
        eosio_exit(0);
    }
}

} // namespace eosio
