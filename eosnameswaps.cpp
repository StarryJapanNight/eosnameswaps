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
    require_auth2(sell_data.account4sale.value, name("owner").value);

    // ----------------------------------------------
    // Valid transaction checks
    // ----------------------------------------------

    // Check an account with that name is not already listed for sale
    auto itr_accounts = _accounts.find(sell_data.account4sale.value);
    eosio_assert(itr_accounts == _accounts.end(), "That account is already for sale.");

    // Check the payment account exists
    eosio_assert(is_account(sell_data.paymentaccnt), "Sell Error: The payment account does not exist.");

    // Check the transfer is valid
    eosio_assert(sell_data.saleprice.symbol == symbol("EOS", 4), "Sell Error: Sale price must be in EOS. Ex: '10.0000 EOS'.");
    eosio_assert(sell_data.saleprice.is_valid(), "Sell Error: Sale price is not valid.");
    eosio_assert(sell_data.saleprice >= asset(10000, symbol("EOS", 4)), "Sell Error: Sale price must be at least 1 EOS. Ex: '1.0000 EOS'.");

    // Check the message is not longer than 100 characters
    eosio_assert(sell_data.message.length() <= 100, "Sell Error: The message must be <= 100 characters.");

    // ----------------------------------------------
    // Change account ownership
    // ----------------------------------------------

    // Change auth from account4sale@active to contract@active
    // This ensures eosio.code permission has been set to the contract
    account_auth(sell_data.account4sale, _self, name("active"), name("owner"), "None");

    // Change auth from contract@owner to owner@owner
    // This ensures the contract is the only owner
    account_auth(sell_data.account4sale, _self, name("owner"), name(""), "None");

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
        s.last_voter = name("");
        s.message = sell_data.message;
    });

    // Place data in bids table. Bidder pays for ram storage
    _bids.emplace(sell_data.account4sale, [&](auto &s) {
        s.account4sale = sell_data.account4sale;
        s.bidaccepted = 1;
        s.bidprice = asset(0, symbol("EOS", 4));
        s.bidder = name("");
    });

    // Place data in stats table. Contract pays for ram storage
    auto itr_stats = _stats.find(0);
    if (itr_stats != _stats.end()) {

        _stats.modify(itr_stats, _self, [&](auto &s) {
            s.num_listed++;
        });

    } else {

        _stats.emplace(_self, [&](auto &s) {
        
            s.index = 0;
            s.num_listed = 0;
            s.num_purchased = 0;
            s.tot_sales = asset(0,symbol("EOS", 4));
            s.tot_fees = asset(0,symbol("EOS", 4));

        });

    }


    // Send message
    send_message(sell_data.paymentaccnt, string("EOSNameSwaps: Your account ") + name{sell_data.account4sale}.to_string() + string(" has been listed for sale. Keep an eye out for bids, and don't forget to vote for accounts you like!"));

}

// Action: Buy an account listed for sale
void eosnameswaps::buy(const transfer_type &transfer_data)
{

    // ----------------------------------------------
    // Auth checks
    // ----------------------------------------------

    // EOSBet hack
    eosio_assert(transfer_data.to == _self || transfer_data.from == _self, "Buy Error: Transfer must be direct to/from us.");

    // ----------------------------------------------
    // Valid transaction checks
    // ----------------------------------------------

    // Check the transfer is valid
    eosio_assert(transfer_data.quantity.symbol == symbol("EOS", 4), "Buy Error: You must pay in EOS.");
    eosio_assert(transfer_data.quantity.is_valid(), "Buy Error: Quantity is not valid.");

    // Find the length of the account name
    int name_length;
    for (int lp = 1; lp <= 12; ++lp)
    {
        if (transfer_data.memo[lp] == ',')
        {
            name_length = lp;
        }
    }

    // Extract account to buy from memo
    const string account_string = transfer_data.memo.substr(0, name_length);
    const name account_to_buy = name(account_string);
   
    eosio_assert(transfer_data.memo[name_length + 1 + KEY_LENGTH] == ',', "Buy Error: New owner and active keys must be supplied.");

    const string owner_key_str = transfer_data.memo.substr(name_length + 1, KEY_LENGTH);
    const string active_key_str = transfer_data.memo.substr(name_length + 2 + KEY_LENGTH, KEY_LENGTH);

    // Check the account is available to buy
    auto itr_accounts = _accounts.find(account_to_buy.value);
    eosio_assert(itr_accounts != _accounts.end(), std::string("Buy Error: Account " + account_string + " is not for sale.").c_str());

    // ----------------------------------------------
    // Sale/Bid price
    // ----------------------------------------------

    // Sale price
    auto saleprice = itr_accounts->saleprice;

    // Check the correct amount of EOS was transferred
    if (transfer_data.quantity != saleprice)
    {
        auto itr_bids = _bids.find(account_to_buy.value);

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
    // Seller, contract, partner, and founder fees
    // ----------------------------------------------

    auto sellerfee = saleprice;
    auto contractfee = saleprice;

    // Fee amounts
    contractfee.amount = int(saleprice.amount * contract_pc);
    sellerfee.amount = saleprice.amount - contractfee.amount;

    action(
        permission_level{_self, name("active")},
        name("eosio.token"), name("transfer"),
        std::make_tuple(_self, feesaccount, contractfee,std::string("EOSNameSwaps: Account contract fee: ") + itr_accounts->account4sale.to_string()))
        .send();

    // Transfer EOS from contract to seller minus the contract fees
    action(
        permission_level{_self, name("active")},
        name("eosio.token"), name("transfer"),
        std::make_tuple(_self, itr_accounts->paymentaccnt, sellerfee, std::string("EOSNameSwaps: Account seller fee: ") + itr_accounts->account4sale.to_string()))
        .send();

    // ----------------------------------------------
    // Update account owner
    // ----------------------------------------------

    // Remove contract@owner permissions and replace with buyer@active account and the supplied key
    account_auth(itr_accounts->account4sale, transfer_data.from, name("active"), name("owner"), active_key_str);

    // Remove seller@active permissions and replace with buyer@owner account and the supplied key
    account_auth(itr_accounts->account4sale, transfer_data.from, name("owner"), name(""), owner_key_str);

    // ----------------------------------------------
    // Cleanup
    // ----------------------------------------------

    // Erase account from the accounts table
    _accounts.erase(itr_accounts);

    // Erase account from the extras table
    auto itr_extras = _extras.find(account_to_buy.value);
    _extras.erase(itr_extras);

    // Erase account from the bids table
    auto itr_bids = _bids.find(account_to_buy.value);
    _bids.erase(itr_bids);

    // Place data in stats table. Contract pays for ram storage
    auto itr_stats = _stats.find(0);
    _stats.modify(itr_stats, _self, [&](auto &s) {
        s.num_listed--;
        s.num_purchased++;
        s.tot_sales += saleprice;
        s.tot_fees += contractfee;
    });

    // Send message
    send_message(transfer_data.from, string("EOSNameSwaps: You have successfully bought the account ") + name{account_to_buy}.to_string() + string(". Please come again."));
}

// Action: Remove a listed account from sale
void eosnameswaps::cancel(const cancel_type &cancel_data)
{

    // ----------------------------------------------
    // Auth checks
    // ----------------------------------------------

    // Check an account with that name is listed for sale
    auto itr_accounts = _accounts.find(cancel_data.account4sale.value);
    eosio_assert(itr_accounts != _accounts.end(), "Cancel Error: That account name is not listed for sale");

    // Only the payment account can cancel the sale (the contract has the owner key)
    eosio_assert(has_auth(itr_accounts->paymentaccnt), "Cancel Error: Only the payment account can cancel the sale.");

    // ----------------------------------------------
    // Update account owners
    // ----------------------------------------------

    // Change auth from contract@active to submitted active key
    account_auth(cancel_data.account4sale, itr_accounts->paymentaccnt, name("active"), name("owner"), cancel_data.active_key_str);

    // Change auth from contract@owner to submitted owner key
    account_auth(cancel_data.account4sale, itr_accounts->paymentaccnt, name("owner"), name(""), cancel_data.owner_key_str);

    // ----------------------------------------------
    // Cleanup
    // ----------------------------------------------

    // Erase account from accounts table
    _accounts.erase(itr_accounts);

    // Erase account from the extras table
    auto itr_extras = _extras.find(cancel_data.account4sale.value);
    _extras.erase(itr_extras);

    // Erase account from the bids table
    auto itr_bids = _bids.find(cancel_data.account4sale.value);
    _bids.erase(itr_bids);

    // Place data in stats table. Contract pays for ram storage
    auto itr_stats = _stats.find(0);
    _stats.modify(itr_stats, _self, [&](auto &s) {
        s.num_listed--;
    });

    // Send message
    send_message(itr_accounts->paymentaccnt, string("EOSNameSwaps: You have successfully cancelled the sale of the account ") + name{cancel_data.account4sale}.to_string() + string(". Please come again."));
}

// Action: Update the sale price
void eosnameswaps::updatesale(const updatesale_type &updatesale_data)
{

    // ----------------------------------------------
    // Auth checks
    // ----------------------------------------------

    // Check an account with that name is listed for sale
    auto itr_accounts = _accounts.find(updatesale_data.account4sale.value);
    eosio_assert(itr_accounts != _accounts.end(), "Update Error: That account name is not listed for sale");

    // Only the payment account can update the sale price
    eosio_assert(has_auth(itr_accounts->paymentaccnt), "Update Error: Only the payment account can update a sale.");

    // ----------------------------------------------
    // Valid transaction checks
    // ----------------------------------------------

    // Check the transfer is valid
    eosio_assert(updatesale_data.saleprice.symbol == symbol("EOS", 4), "Update Error: Sale price must be in EOS. Ex: '10.0000 EOS'.");
    eosio_assert(updatesale_data.saleprice.is_valid(), "Update Error: Sale price is not valid.");
    eosio_assert(updatesale_data.saleprice >= asset(10000, symbol("EOS", 4)), "Update Error: Sale price must be at least 1 EOS. Ex: '1.0000 EOS'.");

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
    auto itr_extras = _extras.find(updatesale_data.account4sale.value);
    _extras.modify(itr_extras, itr_accounts->paymentaccnt, [&](auto &s) {
        s.message = updatesale_data.message;
    });

    // Send message
    send_message(itr_accounts->paymentaccnt, string("EOSNameSwaps: You have successfully updated the sale of the account ") + name{updatesale_data.account4sale}.to_string());
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
    auto itr_extras = _extras.find(vote_data.account4sale.value);
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
    auto itr_bids = _bids.find(proposebid_data.account4sale.value);
    eosio_assert(itr_bids != _bids.end(), "Propose Bid Error: That account name is not listed for sale");

    // Check the transfer is valid
    eosio_assert(proposebid_data.bidprice.symbol == symbol("EOS", 4), "Propose Bid Error: Bid price must be in EOS. Ex: '10.0000 EOS'.");
    eosio_assert(proposebid_data.bidprice.is_valid(), "Propose Bid Error: Bid price is not valid.");
    eosio_assert(proposebid_data.bidprice >= asset(10000, symbol("EOS", 4)), "Propose Bid Error: The minimum bid price is 1.0000 EOS.");

    // Only accept new bids if they are higher
    eosio_assert(proposebid_data.bidprice > itr_bids->bidprice, "Propose Bid Error: You must bid higher than the last bidder.");

    // Only accept new bids if they are lower than the sale price
    auto itr_accounts = _accounts.find(proposebid_data.account4sale.value);
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
    send_message(itr_accounts->paymentaccnt, string("EOSNameSwaps: Your account ") + name{proposebid_data.account4sale}.to_string() + string(" has received a bid. If you choose to accept it, the bidder can purchase the account at the lower price. Others can still bid higher or pay the full sale price until then."));
}

// Action: Accept or decline a bid for an account
void eosnameswaps::decidebid(const decidebid_type &decidebid_data)
{

    // ----------------------------------------------
    // Auth checks
    // ----------------------------------------------

    // Check an account with that name is listed for sale
    auto itr_accounts = _accounts.find(decidebid_data.account4sale.value);
    eosio_assert(itr_accounts != _accounts.end(), "Decide Bid Error: That account name is not listed for sale.");

    // Only the payment account can accept bids
    eosio_assert(has_auth(itr_accounts->paymentaccnt), "Decide Bid Error: Only the payment account can decide on bids.");

    // ----------------------------------------------
    // Valid transaction checks
    // ----------------------------------------------

    auto itr_bids = _bids.find(decidebid_data.account4sale.value);

    // Check there is a bid to accept or reject
    eosio_assert(itr_bids->bidprice != asset(0, symbol("EOS", 4)), "Decide Bid Error: There are no bids to accept or reject.");

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
        send_message(itr_bids->bidder, string("EOSNameSwaps: Your bid for ") + name{decidebid_data.account4sale}.to_string() + string(" has been accepted. Account ") + name{itr_bids->bidder}.to_string() + string(" can buy it for the bid price. Be quick, as others can still outbid you or pay the full sale price."));
    }
    else
    {

        // Bid rejected
        _bids.modify(itr_bids, itr_accounts->paymentaccnt, [&](auto &s) {
            s.bidaccepted = 0;
        });

        // Send message
        send_message(itr_bids->bidder, string("EOSNameSwaps: Your bid for ") + name{decidebid_data.account4sale}.to_string() + string(" has been rejected. Increase your bid offer"));
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
void eosnameswaps::screener(const screener_type &screener_data)
{

    // ----------------------------------------------
    // Auth checks
    // ----------------------------------------------

    // Only the contract accounts can perform screening
    require_auth(_self);

    // ----------------------------------------------
    // Set the screening status of an account listed for sale
    // ----------------------------------------------

    int screened = screener_data.option;

    eosio_assert(screened == 0 || screened == 1, "Admin Error: Malformed screening data.");

    // Place data in table. Contract pays for ram storage
    auto itr_extras = _extras.find(screener_data.account4sale.value);
    _extras.modify(itr_extras, _self, [&](auto &s) {
        s.screened = screened;
    });

    // ----------------------------------------------
}

// Broadcast message
void eosnameswaps::send_message(eosio::name receiver, string message)
{

    action(permission_level{_self, name("active")},
           name("eosnameswaps"), name("message"),
           std::make_tuple(
               receiver,
               message))
        .send();
}

// Changes the owner/active permissions
void eosnameswaps::account_auth(eosio::name account4sale, eosio::name changeto, eosio::name perm_child, eosio::name perm_parent, string pubkey_str)
{

    // Setup authority for contract. Choose either a new key, or account, or both.
    authority contract_authority;
    if (pubkey_str != "None")
    {
        // Convert the public key string to the requried type
        const abieos::public_key pubkey = abieos::string_to_public_key(pubkey_str);

        // Array to hold public key
        std::array<char, 33> pubkey_char;
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
    action(permission_level{account4sale, name("owner")},
           name("eosio"), name("updateauth"),
           std::make_tuple(
               account4sale,
               perm_child,
               perm_parent,
               contract_authority))
        .send();
} // namespace eosio


// Loan CPU/NET to seller 
void eosnameswaps::lend(const lend_type &lend_data)
{

    // ----------------------------------------------
    // Auth checks
    // ----------------------------------------------

    // Only the this permission can init a loan
    require_auth2(_self.value,name("initloan").value);

    // ----------------------------------------------
    // Valid transaction checks
    // ----------------------------------------------

    // Limit lending to 5/1 EOS for CPU/NET
    eosio_assert(lend_data.cpu.amount <= 50000 && lend_data.net.amount <= 10000,"Lend Error: Max loan of 5 EOS for CPU and 1 EOS for NET.");

    // ----------------------------------------------
    // Lend bandwidth to account4sale
    // ----------------------------------------------

    // Current time in seconds
    auto timenow = now();

    // Where in four day period?
    float fourdays = timenow / (3600.0 * 24.0 * 4.0);
    float frac = fourdays - long(fourdays);

    // Select which loan account to use (rotates every 4 days)
    name loan_account;
    if (frac < 0.25)
    {
        loan_account = name("nameswapsln1");
    }
    else if (frac >= 0.25 && frac < 0.50)
    {
        loan_account = name("nameswapsln2");
    }
    else if (frac >= 0.50 && frac < 0.75)
    {
        loan_account = name("nameswapsln3");
    }
    else if (frac > 0.75)
    {
        loan_account = name("nameswapsln4");
    }

    action(
        permission_level{loan_account, name("loaner")},
        name("eosio"), name("delegatebw"),
        std::make_tuple(loan_account, lend_data.account4sale, lend_data.net, lend_data.cpu, false))
        .send();

    print("Lended");

    // ----------------------------------------------
    // Unlend bandwidth (deferred)
    // ----------------------------------------------

    eosio::transaction t{};

    t.actions.emplace_back(
        permission_level{loan_account, name("loaner")},
        name("eosio"), name("undelegatebw"),
        std::make_tuple(loan_account,  lend_data.account4sale, lend_data.net, lend_data.cpu))
        .send();

    t.delay_sec = 10;

    // use now() as sender id for ease of use
    t.send(now(), _self);

}

extern "C"
{
    void apply(uint64_t receiver, uint64_t code, uint64_t action)
    {

        if (code == name("eosio.token").value && action == name("transfer").value)
        {
            execute_action(name(receiver), name(code), &eosnameswaps::buy);
        }
        else if (code == receiver && action == name("sell").value)
        {
            execute_action(name(receiver), name(code), &eosnameswaps::sell);
        }
        else if (code == receiver && action == name("cancel").value)
        {
            execute_action(name(receiver), name(code), &eosnameswaps::cancel);
        }
        else if (code == receiver && action == name("updatesale").value)
        {
            execute_action(name(receiver), name(code), &eosnameswaps::updatesale);
        }
        else if (code == receiver && action == name("vote").value)
        {
            execute_action(name(receiver), name(code), &eosnameswaps::vote);
        }
        else if (code == receiver && action == name("proposebid").value)
        {
            execute_action(name(receiver), name(code), &eosnameswaps::proposebid);
        }
        else if (code == receiver && action == name("decidebid").value)
        {
            execute_action(name(receiver), name(code), &eosnameswaps::decidebid);
        }
        else if (code == receiver && action == name("message").value)
        {
            execute_action(name(receiver), name(code), &eosnameswaps::message);
        }
        else if (code == receiver && action == name("screener").value)
        {
            execute_action(name(receiver), name(code), &eosnameswaps::screener);
        }
        eosio_exit(0);
    }
}

} // namespace eosio
