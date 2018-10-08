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
    eosio_assert(itr_usr->cpu_weight.amount >= 5000, "You can not sell an account with less than 0.5 EOS staked to CPU. Contract actions will fail with less.");
    eosio_assert(itr_usr->net_weight.amount >= 1000, "You can not sell an account with less than 0.1 EOS staked to NET. Contract actions will fail with less.");

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
        s.numberofvotes = 0;
        s.last_voter = N();
        s.message = sell_data.message;
    });

    // Place data in bids table. Bidder pays for ram storage
    _bids.emplace(sell_data.account4sale, [&](auto &s) {
        s.account4sale = sell_data.account4sale;
        s.bidaccepted = 1;
        s.bidprice = asset(0);
        s.bidder = N();
    });

    // Place data in stats table. Contract pays for ram storage
    auto itr_stats = _stats.find(0);
    _stats.modify(itr_stats, _self, [&](auto &s) {
        s.num_listed++;
    });

    // Send message
    send_message(sell_data.paymentaccnt, string("EOSNAMESWAPS: Your account ") + name{sell_data.account4sale}.to_string() + string(" has been listed for sale. Keep an eye out for bids, and don't forget to vote for accounts you like!"));
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

    eosio_assert(transfer_data.memo[name_length + KEY_LENGTH] == ',', "Buy Error: New owner and active keys must be supplied.");

    const string owner_key_str = transfer_data.memo.substr(name_length, KEY_LENGTH);
    const string active_key_str = transfer_data.memo.substr(name_length + 1 + KEY_LENGTH, KEY_LENGTH);

    // Check the account is available to buy
    auto itr_accounts = _accounts.find(account_to_buy);
    eosio_assert(itr_accounts != _accounts.end(), std::string("Buy Error: Account " + name{account_to_buy}.to_string() + " is not for sale.").c_str());

    // ----------------------------------------------
    // Sale/Bid price
    // ----------------------------------------------

    // Sale price
    auto saleprice = itr_accounts->saleprice;

    // Check the correct amount of EOS was transferred
    if (transfer_data.quantity != saleprice)
    {
        auto itr_bids = _bids.find(account_to_buy);

        // Current bid decision by seller
        const int bid_decision = itr_bids->bidaccepted;

        if (transfer_data.quantity == itr_bids->bidprice)
        {

            // Check the bid has been accepted
            if (bid_decision == BID_ACCEPTED)
            {
                // Check the bid has not been rejected
                eosio_assert(bid_decision != BID_REJECTED, "Buy Error: The bid has been rejected. Bid higher.");

                // Check the bid has been decided
                eosio_assert(bid_decision != BID_UNDECIDED, "Buy Error: The bid has not been accepted or rejected yet.");
            }

            // Check the bid is from the accepted bidder
            eosio_assert(itr_bids->bidder == transfer_data.from, "Buy Error: Only the accepted bidder can purchase the account at the bid price.");

            // Lower sale price to the bid price for the bidder only
            saleprice = itr_bids->bidprice;
        }
    }

    eosio_assert(saleprice == transfer_data.quantity, "Buy Error: You have not transferred the correct amount of EOS. Check the sale price.");

    // ----------------------------------------------
    // Seller, contract, and referrer fees
    // ----------------------------------------------

    account_name referrer_name;
    bool isreferrer = false;

    // Check for a referrer name in the memo
    if (transfer_data.memo[name_length + 2 * KEY_LENGTH + 1] == ',')
    {
        // A referrer has been added
        int referrer_len = transfer_data.memo.length() - (name_length + 2 * KEY_LENGTH + 2);

        string referrer_str = transfer_data.memo.substr(name_length + 2 * KEY_LENGTH + 2, referrer_len);
        referrer_name = string_to_name(referrer_str.c_str());

        // Check the referrer is known and approved
        if (referrer_name == N(hellodidieos) || referrer_name == N(avocadocream))
        {
            isreferrer = true;
        }
    }

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

    // Erase account from the bids table
    auto itr_bids = _bids.find(account_to_buy);
    _bids.erase(itr_bids);

    // Place data in stats table. Contract pays for ram storage
    auto itr_stats = _stats.find(0);
    _stats.modify(itr_stats, _self, [&](auto &s) {
        s.num_listed--;
        s.num_purchased++;
        s.tot_sales += saleprice;
        s.tot_fees += contractfee + referrerfee;
    });

    // Send message
    send_message(transfer_data.from, string("EOSNAMESWAPS: You have successfully bought the account ") + name{account_to_buy}.to_string() + string(". Please come again."));
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
    eosio_assert(has_auth(itr_accounts->paymentaccnt), "Cancel Error: Only the payment account can cancel the sale.");

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

    // Erase account from the bids table
    auto itr_bids = _bids.find(cancel_data.account4sale);
    _bids.erase(itr_bids);

    // Place data in stats table. Contract pays for ram storage
    auto itr_stats = _stats.find(0);
    _stats.modify(itr_stats, _self, [&](auto &s) {
        s.num_listed--;
    });

    // Send message
    send_message(itr_accounts->paymentaccnt, string("EOSNAMESWAPS: You have successfully cancelled the sale of the account ") + name{cancel_data.account4sale}.to_string() + string(". Please come again."));
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
    eosio_assert(has_auth(itr_accounts->paymentaccnt), "Update Error: Only the payment account can update a sale.");

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
    // Update tables
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

    // Send message
    send_message(itr_accounts->paymentaccnt, string("EOSNAMESWAPS: You have successfully updated the sale of the account ") + name{updatesale_data.account4sale}.to_string());
}

// Action: Increment votes
void eosnameswaps::vote(const vote_type &vote_data)
{

    // ----------------------------------------------
    // Auth checks
    // ----------------------------------------------

    // Confirm the voter is who they say they are
    eosio_assert(has_auth(vote_data.voter), "Vote Error: You are not who you say you are. Check permissions.");

    // ----------------------------------------------
    // Valid transaction checks
    // ----------------------------------------------

    // Check an account with that name is listed for sale
    auto itr_extras = _extras.find(vote_data.account4sale);
    eosio_assert(itr_extras != _extras.end(), "Vote Error: That account name is not listed for sale.");

    // Can only vote once in a row
    eosio_assert(vote_data.voter != itr_extras->last_voter, "Vote Error: You have already voted for this account!");

    // ----------------------------------------------
    // Update table
    // ----------------------------------------------

    // Place data in extras table. Voter pays for ram storage
    _extras.modify(itr_extras, vote_data.voter, [&](auto &s) {
        s.numberofvotes++;
        s.last_voter = vote_data.voter;
    });
}

// Action: Propose a bid for an account
void eosnameswaps::proposebid(const proposebid_type &proposebid_data)
{

    // ----------------------------------------------
    // Auth checks
    // ----------------------------------------------

    // Confirm the bidder is who they say they are
    eosio_assert(has_auth(proposebid_data.bidder), "Propose Bid Error: You are not who you say you are. Check permissions.");

    // ----------------------------------------------
    // Valid transaction checks
    // ----------------------------------------------

    // Check an account with that name is listed for sale
    auto itr_bids = _bids.find(proposebid_data.account4sale);
    eosio_assert(itr_bids != _bids.end(), "Propose Bid Error: That account name is not listed for sale");

    // Check the transfer is valid
    eosio_assert(proposebid_data.bidprice.symbol == string_to_symbol(4, "EOS"), "Propose Bid Error: Bid price must be in EOS. Ex: '10.0000 EOS'.");
    eosio_assert(proposebid_data.bidprice.is_valid(), "Propose Bid Error: Bid price is not valid.");
    eosio_assert(proposebid_data.bidprice >= asset(10000), "Propose Bid Error: The minimum bid price is 1.0000 EOS.");

    // Only accept new bids if they are higher
    eosio_assert(proposebid_data.bidprice > itr_bids->bidprice, "Propose Bid Error: You must bid higher than the last bidder.");

    // Only accept new bids if they are lower than the sale price
    auto itr_accounts = _accounts.find(proposebid_data.account4sale);
    eosio_assert(proposebid_data.bidprice <= itr_accounts->saleprice, "Propose Bid Error: You must bid lower than the sale price.");

    // ----------------------------------------------
    // Update table
    // ----------------------------------------------

    // Place data in bids table. Bidder pays for ram storage
    _bids.modify(itr_bids, proposebid_data.bidder, [&](auto &s) {
        s.bidaccepted = 1;
        s.bidprice = proposebid_data.bidprice;
        s.bidder = proposebid_data.bidder;
    });

    // Send message
    send_message(itr_accounts->paymentaccnt, string("EOSNAMESWAPS: Your account ") + name{proposebid_data.account4sale}.to_string() + string(" has received a bid. If you choose to accept it, the bidder can purchase the account at the lower price. Others can still bid higher or pay the full sale price until then."));
}

// Action: Accept or decline a bid for an account
void eosnameswaps::decidebid(const decidebid_type &decidebid_data)
{

    // ----------------------------------------------
    // Auth checks
    // ----------------------------------------------

    // Check an account with that name is listed for sale
    auto itr_accounts = _accounts.find(decidebid_data.account4sale);
    eosio_assert(itr_accounts != _accounts.end(), "Decide Bid Error: That account name is not listed for sale.");

    // Only the payment account can accept bids
    eosio_assert(has_auth(itr_accounts->paymentaccnt), "Decide Bid Error: Only the payment account can decide on bids.");

    // ----------------------------------------------
    // Valid transaction checks
    // ----------------------------------------------

    auto itr_bids = _bids.find(decidebid_data.account4sale);

    // Check there is a bid to accept or reject
    eosio_assert(itr_bids->bidprice != asset(0), "Decide Bid Error: There are no bids to accept or reject.");

    // ----------------------------------------------
    // Update table
    // ----------------------------------------------

    // Place data in bids table. Bidder pays for ram storage
    if (decidebid_data.accept == true)
    {

        // Bid accepted
        _bids.modify(itr_bids, itr_accounts->paymentaccnt, [&](auto &s) {
            s.bidaccepted = 2;
        });

        // Send message
        send_message(itr_bids->bidder, string("EOSNAMESWAPS: Your bid for ") + name{decidebid_data.account4sale}.to_string() + string(" has been accepted. Account ") + name{itr_bids->bidder}.to_string() + string(" can buy it for the bid price. Be quick, as others can still outbid you or pay the full sale price."));
    }
    else
    {

        // Bid rejected
        _bids.modify(itr_bids, itr_accounts->paymentaccnt, [&](auto &s) {
            s.bidaccepted = 0;
        });

        // Send message
        send_message(itr_bids->bidder, string("EOSNAMESWAPS: Your bid for ") + name{decidebid_data.account4sale}.to_string() + string(" has been rejected. Increase your bid offer"));
    }
}

// Message Action
void eosnameswaps::message(const message_type &message_data)
{

    // ----------------------------------------------
    // Auth checks
    // ----------------------------------------------

    // Only the contract can send a message
    eosio_assert(has_auth(_self), "Message Error: Only the contract can send messages.");

    // ----------------------------------------------

    // Notify the specified account
    require_recipient(message_data.receiver);
}

// Action: Perform admin tasks
void eosnameswaps::admin(const admin_type &admin_data)
{

    // ----------------------------------------------
    // Auth checks
    // ----------------------------------------------

    // Only the contract can update an accounts auth
    eosio_assert(has_auth(_self), "Admin Error: Only the contract owner can perform admin actions.");

    // ----------------------------------------------
    // Valid transaction checks
    // ----------------------------------------------

    // Check the action is valid
    eosio_assert(admin_data.action == string("screened") || admin_data.action == string("inittable"), "Admin Error: Not a valid action.");

    // ----------------------------------------------
    // Perform admin actions
    // ----------------------------------------------

    if (admin_data.action == string("screened"))
    {

        // ----------------------------------------------
        // Set the screening status of an account listed for sale
        // ----------------------------------------------

        int screened = std::stoi(admin_data.option);

        eosio_assert(screened == 0 || screened == 1, "Admin Error: Malformed screening data.");

        // Place data in table. Contract pays for ram storage
        auto itr_extras = _extras.find(admin_data.account4sale);
        _extras.modify(itr_extras, _self, [&](auto &s) {
            s.screened = screened;
        });

        // ----------------------------------------------
    }

    /*
    if (admin_data.action == string("copytable"))
    {

        // ----------------------------------------------
        // Upgrade tables
        // ----------------------------------------------

        for (auto &item : _accounts)
        {

            // Place data in bids table. Contract pays for ram storage
            _bids.emplace(_self, [&](auto &s) {
                s.account4sale = item.account4sale;
                s.bidaccepted = 1;
                s.bidprice = asset(0);
                s.bidder = N();
            });
        }

        // Delete the old table
        for (auto &item : _isforsale)
        {
            _isforsale.erase(item);
        }
        
        // ----------------------------------------------
    }
    */

    if (admin_data.action == string("inittable"))
    {

        // ----------------------------------------------
        // Upgrade tables
        // ----------------------------------------------

        // Place data in stats table. Contract pays for ram storage
        _stats.emplace(_self, [&](auto &s) {
            s.index = 0;
            s.num_listed = 173;
            s.num_purchased = 1;
            s.tot_sales = asset(1224 * 10000);
            s.tot_fees = asset(2 * 12.24 * 10000);
        });

        // ----------------------------------------------
    }
}

void eosnameswaps::send_message(account_name receiver, string message)
{

    action(permission_level{_self, N(owner)},
           N(eosnameswaps), N(message),
           std::make_tuple(
               receiver,
               message))
        .send();
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
    case N(vote):
        vote(unpack_action_data<vote_type>());
        break;
    case N(proposebid):
        proposebid(unpack_action_data<proposebid_type>());
        break;
    case N(decidebid):
        decidebid(unpack_action_data<decidebid_type>());
        break;
    case N(message):
        message(unpack_action_data<message_type>());
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
        case N(vote):
            _eosnameswaps.vote(unpack_action_data<vote_type>());
            break;
        case N(proposebid):
            _eosnameswaps.proposebid(unpack_action_data<proposebid_type>());
            break;
        case N(decidebid):
            _eosnameswaps.decidebid(unpack_action_data<decidebid_type>());
            break;
        case N(message):
            _eosnameswaps.message(unpack_action_data<message_type>());
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
