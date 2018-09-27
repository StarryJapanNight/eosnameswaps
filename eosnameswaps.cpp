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

    // ----------------------------------------------
    // Auth checks
    // ----------------------------------------------

    // Only the account4sale@owner can sell (contract@eosio.code must already be an owner)
    require_auth2(sell_data.account4sale, N(owner));

    // ----------------------------------------------
    // Valid transaction checks
    // ----------------------------------------------

    // Check an account with that name is not already listed for sale
    auto itr_accounts = _accounts.find(sell_data.account4sale);
    eosio_assert(itr_accounts == _accounts.end(), "That account is already for sale.");

    // Check the payment account exists
    eosio_assert(is_account(sell_data.paymentaccnt), "Sell Error: The payment account does not exist.");

    // Check the transfer is valid
    eosio_assert(sell_data.saleprice.symbol == string_to_symbol(4, "EOS"), "Sell Error: Sale price must be in EOS. Ex: '10.0000 EOS'.");
    eosio_assert(sell_data.saleprice.is_valid(), "Sell Error: Sale price is not valid.");
    eosio_assert(sell_data.saleprice >= asset(10000), "Sell Error: Sale price must be at least 1 EOS. Ex: '1.0000 EOS'.");

    // Check the message is not longer than 100 characters
    eosio_assert(sell_data.message.length() <= 100, "Sell Error: The message must be <= 100 characters.");

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

    // ----------------------------------------------
    // Add data to tables
    // ----------------------------------------------

    // Place data in accounts table. Seller pays for ram storage
    _accounts.emplace(sell_data.account4sale, [&](auto &s) {
        s.account4sale = sell_data.account4sale;
        s.saleprice = sell_data.saleprice;
        s.paymentaccnt = sell_data.paymentaccnt;
    });

    // Place data in extras table. Seller pays for ram storage
    _extras.emplace(sell_data.account4sale, [&](auto &s) {
        s.account4sale = sell_data.account4sale;
        s.screened = false;
        s.numberoflikes = 0;
        s.last_liked = N();
        s.message = sell_data.message;
    });
}

// Action: Buy an account listed for sale
void eosnameswaps::buy(const currency::transfer &transfer_data)
{

    // ----------------------------------------------
    // Auth checks
    // ----------------------------------------------

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

    eosio_assert(transfer_data.memo[name_length + 53] == ',', "Buy Error: New owner and active keys must be supplied.");

    const string owner_key_str = transfer_data.memo.substr(name_length, 53);
    const string active_key_str = transfer_data.memo.substr(name_length + 54, 53);

    // Check the account is available to buy
    auto itr_accounts = _accounts.find(account_to_buy);
    eosio_assert(itr_accounts != _accounts.end(), std::string("Buy Error: Account " + name{account_to_buy}.to_string() + " is not for sale.").c_str());

    // Check the correct amount of EOS was transferred
    eosio_assert(itr_accounts->saleprice.amount == transfer_data.quantity.amount, "Buy Error: You have not transferred the correct amount of EOS. Check the sale price.");

    // ----------------------------------------------
    // Seller, contract, and referrer fees
    // ----------------------------------------------

    account_name referrer_name;
    bool isreferrer = false;

    // Check for a referrer name in the memo
    if (transfer_data.memo[name_length + 107] == ',')
    {
        // A referrer has been added
        int referrer_len = transfer_data.memo.length() - (name_length + 108);

        string referrer_str = transfer_data.memo.substr(name_length + 108, referrer_len);
        referrer_name = string_to_name(referrer_str.c_str());

        // Check the referrer is known and approved
        if (referrer_name == N(hellodidieos) || referrer_name == N(avocadocream))
        {
            isreferrer = true;
        }
    }

    // Sale price
    auto saleprice = itr_accounts->saleprice;

    // Contract and referrer fees
    auto contractfee = saleprice;
    auto referrerfee = saleprice;
    auto sellerfee = saleprice;

    if (isreferrer)
    {

        // Referrer present - Referrer gets 50% of the sale fee
        contractfee.amount = 0.5 * get_salefee() * saleprice.amount;
        referrerfee.amount = contractfee.amount;
        sellerfee.amount = saleprice.amount - contractfee.amount - referrerfee.amount;

        // Transfer EOS from contract to referrers account
        action(
            permission_level{_self, N(owner)},
            N(eosio.token), N(transfer),
            std::make_tuple(_self, referrer_name, referrerfee, std::string("EOSNAMESWAPS: Account sale referrer fee: " + name{itr_accounts->account4sale}.to_string())))
            .send();
    }
    else
    {

        // No referrer - Contract gets all of the sale fee
        contractfee.amount = get_salefee() * saleprice.amount;
        referrerfee.amount = 0.0;
        sellerfee.amount = saleprice.amount - contractfee.amount;
    }

    // Transfer EOS from contract to contract fees account
    action(
        permission_level{_self, N(owner)},
        N(eosio.token), N(transfer),
        std::make_tuple(_self, get_contractfees(), contractfee, std::string("EOSNAMESWAPS: Account sale contract fee: " + name{itr_accounts->account4sale}.to_string())))
        .send();

    // Transfer EOS from contract to seller minus the contract fee
    action(
        permission_level{_self, N(owner)},
        N(eosio.token), N(transfer),
        std::make_tuple(_self, itr_accounts->paymentaccnt, sellerfee, std::string("EOSNAMESWAPS: Account sale fee: " + name{itr_accounts->account4sale}.to_string())))
        .send();

    // ----------------------------------------------
    // Update account owner
    // ----------------------------------------------

    // Remove contract@owner permissions and replace with buyer@active account and the supplied key
    account_auth(itr_accounts->account4sale, transfer_data.from, N(active), N(owner), active_key_str);

    // Remove seller@active permissions and replace with buyer@owner account and the supplie key
    account_auth(itr_accounts->account4sale, transfer_data.from, N(owner), N(), owner_key_str);

    // ----------------------------------------------
    // Cleanup
    // ----------------------------------------------

    // Erase account from the accounts table
    _accounts.erase(itr_accounts);

    // Erase account from the extras table
    auto itr_extras = _extras.find(account_to_buy);
    _extras.erase(itr_extras);
}

// Action: Remove a listed account from sale
void eosnameswaps::cancel(const cancel_type &cancel_data)
{

    // ----------------------------------------------
    // Auth checks
    // ----------------------------------------------

    // Check an account with that name is listed for sale
    auto itr_accounts = _accounts.find(cancel_data.account4sale);
    eosio_assert(itr_accounts != _accounts.end(), "Cancel Error: That account name is not listed for sale");

    // Only the payment account can cancel the sale (the contract has the owner key)
    eosio_assert(has_auth(_self) || has_auth(itr_accounts->paymentaccnt), "Cancel Error: Only the payment account can cancel the sale.");

    // ----------------------------------------------
    // Update account owners
    // ----------------------------------------------

    // Change auth from contract@active to submitted active key
    account_auth(cancel_data.account4sale, itr_accounts->paymentaccnt, N(active), N(owner), cancel_data.active_key_str);

    // Change auth from contract@owner to submitted owner key
    account_auth(cancel_data.account4sale, itr_accounts->paymentaccnt, N(owner), N(), cancel_data.owner_key_str);

    // ----------------------------------------------
    // Cleanup
    // ----------------------------------------------

    // Erase account from accounts table
    _accounts.erase(itr_accounts);

    // Erase account from the extras table
    auto itr_extras = _extras.find(cancel_data.account4sale);
    _extras.erase(itr_extras);
}

// Action: Update the sale price
void eosnameswaps::updatesale(const updatesale_type &updatesale_data)
{

    // ----------------------------------------------
    // Auth checks
    // ----------------------------------------------

    // Check an account with that name is listed for sale
    auto itr_accounts = _accounts.find(updatesale_data.account4sale);
    eosio_assert(itr_accounts != _accounts.end(), "Update Error: That account name is not listed for sale");

    // Only the payment account can update the sale price
    require_auth(itr_accounts->paymentaccnt);

    // ----------------------------------------------
    // Valid transaction checks
    // ----------------------------------------------

    // Check the transfer is valid
    eosio_assert(updatesale_data.saleprice.symbol == string_to_symbol(4, "EOS"), "Update Error: Sale price must be in EOS. Ex: '10.0000 EOS'.");
    eosio_assert(updatesale_data.saleprice.is_valid(), "Update Error: Sale price is not valid.");
    eosio_assert(updatesale_data.saleprice >= asset(10000), "Update Error: Sale price must be at least 1 EOS. Ex: '1.0000 EOS'.");

    // Check the message is not longer than 100 characters
    eosio_assert(updatesale_data.message.length() <= 100, "Sell Error: The message must be <= 100 characters.");

    // ----------------------------------------------
    // Update table
    // ----------------------------------------------

    // Place data in accounts table. Payment account pays for ram storage
    _accounts.modify(itr_accounts, itr_accounts->paymentaccnt, [&](auto &s) {
        s.saleprice = updatesale_data.saleprice;
    });

    // Place data in extras table. Payment account pays for ram storage
    auto itr_extras = _extras.find(updatesale_data.account4sale);
    _extras.modify(itr_extras, itr_accounts->paymentaccnt, [&](auto &s) {
        s.message = updatesale_data.message;
    });
}

// Action: Increment likes
void eosnameswaps::likes(const likes_type &likes_data)
{

    // ----------------------------------------------
    // Auth checks
    // ----------------------------------------------

    // Confirm the liker is who they say they are
    require_auth(likes_data.liker);

    // ----------------------------------------------
    // Valid transaction checks
    // ----------------------------------------------

    // Check an account with that name is listed for sale
    auto itr_extras = _extras.find(likes_data.account4sale);
    eosio_assert(itr_extras != _extras.end(), "Likes Error: That account name is not listed for sale");

    // Can only like once in a row
    eosio_assert(likes_data.liker != itr_extras->last_liked, "Likes Error: You already liked this account!");

    // ----------------------------------------------
    // Update table
    // ----------------------------------------------

    // Place data in extras table. Liker pays for ram storage
    _extras.modify(itr_extras, likes_data.liker, [&](auto &s) {
        s.numberoflikes++;
        s.last_liked = likes_data.liker;
    });
}

// Action: Perform admin tasks
void eosnameswaps::admin(const admin_type &admin_data)
{

    // ----------------------------------------------
    // Auth checks
    // ----------------------------------------------

    // Only the contract can update an accounts auth
    require_auth(_self);

    // ----------------------------------------------
    // Valid transaction checks
    // ----------------------------------------------

    // Find the account in the extras tables
    auto itr_extras = _extras.find(admin_data.account4sale);

    // Check the action is valid
    eosio_assert(admin_data.action == string("reauth") || admin_data.action == string("screened") || admin_data.action == string("copytable"), "Admin Error: Not a valid action.");

    // ----------------------------------------------
    // Perform admin actions
    // ----------------------------------------------

    if (admin_data.action == string("reauth"))
    {

        /*
        // ----------------------------------------------
        // Reset the owner of an account to the contract. Helps to fix testing issues.
        // ----------------------------------------------

        // Change auth from account4sale@active to contract@active
        // This ensures eosio.code permission has been set to the contract
        account_auth(admin_data.account4sale, _self, N(active), N(owner), "None");

        // Change auth from contract@owner to owner@owner
        // This ensures the contract is the only owner
        account_auth(admin_data.account4sale, _self, N(owner), N(), "None");
        */

        // ----------------------------------------------
    }
    else if (admin_data.action == string("screened"))
    {

        // ----------------------------------------------
        // Set the screening status of an account listed for sale
        // ----------------------------------------------

        int screened = std::stoi(admin_data.option);

        eosio_assert(screened == 0 || screened == 1, "Admin Error: Malformed screening data.");

        // Place data in table. Contract pays for ram storage
        _extras.modify(itr_extras, _self, [&](auto &s) {
            s.screened = screened;
        });

        // ----------------------------------------------
    }
    else if (admin_data.action == string("copytable"))
    {

        // ----------------------------------------------
        // Upgrade tables
        // ----------------------------------------------

        /*for (auto & item : _accounts)
        {
            // Place data in extras table. Contract pays for ram storage
            _extras.emplace(_self, [&](auto &s) {
                s.account4sale = item.account4sale;
                s.screened = true;
                s.numberoflikes = 0;
                s.last_liked = N();
                s.message = string("");
            });
        }

        // Delete the old table
        for (auto &item : _isforsale)
        {
            _isforsale.erase(item);
        }
        */

        // ----------------------------------------------
    }
}

// Changes the owner/active permissions
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

// Apply function
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
    case N(likes):
        likes(unpack_action_data<likes_type>());
        break;
    case N(admin):
        admin(unpack_action_data<admin_type>());
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
        case N(likes):
            _eosnameswaps.likes(unpack_action_data<likes_type>());
            break;
        case N(admin):
            _eosnameswaps.admin(unpack_action_data<admin_type>());
            break;
        default:
            break;
        }
        eosio_exit(0);
    }
}

} // namespace eosio
