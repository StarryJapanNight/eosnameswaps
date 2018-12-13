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
    _stats.modify(itr_stats, _self, [&](auto &s) {
        s.num_listed++;
    });
    
    // Send message
    send_message(sell_data.paymentaccnt, string("EOSNameSwaps: Your account ") + name{sell_data.account4sale}.to_string() + string(" has been listed for sale. Keep an eye out for bids, and don't forget to vote for accounts you like!"));
}

// Action: Buy an account listed for sale
void eosnameswaps::buy(const transfer_type &transfer_data)
{

    // ----------------------------------------------
    // Auth checks
    // ----------------------------------------------

    // Important: The transfer fees actions below will trigger this function without this
    if (transfer_data.from == _self) return;

    // EOSBet hack
    eosio_assert(transfer_data.to == _self, "Buy Error: Transfer must be direct to contract.");

    // ----------------------------------------------
    // Valid transaction checks
    // ----------------------------------------------

    // Check the buy code is valid
    const string buy_code = transfer_data.memo.substr(0, 3);
    eosio_assert(buy_code == "cn:" || buy_code == "sp:", "Buy Error: Malformed buy string.");

    // Check the transfer is valid
    eosio_assert(transfer_data.quantity.symbol == symbol("EOS", 4), "Buy Error: You must pay in EOS.");
    eosio_assert(transfer_data.quantity.is_valid(), "Buy Error: Quantity is not valid.");

    // Strip buy code from memo
    const string memo = transfer_data.memo.substr(3);

    // Find the length of the account name
    int name_length = 0;
    for (int lp = 1; lp <= 12; ++lp)
    {
        if (memo[lp] == ',')
        {
            name_length = lp;
            break;
        }
    }

    // Check the name length is valid
    eosio_assert(name_length > 0, "Buy Error: Malformed buy name.");

    // Extract account to buy from memo
    const name account_name = name(memo.substr(0, name_length));

    // Extract keys
    const string owner_key = memo.substr(name_length + 1, KEY_LENGTH);
    const string active_key = memo.substr(name_length + 2 + KEY_LENGTH, KEY_LENGTH);

    string referrer = "";
    if (memo.length() > name_length + 3 + 2 * KEY_LENGTH && memo.length() <= name_length + 3 + 2 * KEY_LENGTH + 12)
    {
        referrer = memo.substr(name_length + 3 + 2 * KEY_LENGTH);
    }

    // Call the requried function
    if (buy_code == "cn:")
    {
        buy_custom(account_name, transfer_data.from, transfer_data.quantity, owner_key, active_key);
    }
    else if (buy_code == "sp:")
    {
        buy_saleprice(account_name, transfer_data.from, transfer_data.quantity, owner_key, active_key, referrer);
    }
}

void eosnameswaps::buy_custom(const name account_name, const name from, const asset quantity, const string owner_key, const string active_key)
{

    // Account name length
    int name_length = account_name.length();

    // Extract suffix
    string suffix = (account_name).to_string().substr(name_length - 2, 2);

    // Currently supported suffixes
    eosio_assert(suffix == ".e" || suffix == ".x" || suffix == ".y" || suffix == ".z", "Custom Error: That is not a valid suffix.");

    // Custom name saleprice
    asset saleprice = asset(0, symbol("EOS", 4));

    if (suffix == ".e")
    {

        switch (name_length)
        {
        case 7:
            saleprice.amount = 57000;
            break;
        case 8:
            saleprice.amount = 47000;
            break;
        case 9:
            saleprice.amount = 37000;
            break;
        case 10:
            saleprice.amount = 27000;
            break;
        case 11:
            saleprice.amount = 17000;
            break;
        case 12:
            saleprice.amount = 8000;
            break;
        default:
            eosio_assert(1==0, "Custom Error: Incorrect custom name length");
            break;
        }

    } 
    else if (suffix == ".x")
    {

        switch (name_length)
        {
        case 7:
            saleprice.amount = 67000;
            break;
        case 8:
            saleprice.amount = 57000;
            break;
        case 9:
            saleprice.amount = 47000;
            break;
        case 10:
            saleprice.amount = 37000;
            break;
        case 11:
            saleprice.amount = 27000;
            break;
        case 12:
            saleprice.amount = 17000;
            break;
        default:
            eosio_assert(1==0, "Custom Error: Incorrect custom name length");
            break;
        }

    } 
    else if (suffix == ".y" || suffix == ".z") 
    {

        switch (name_length)
        {
        case 6:
            saleprice.amount = 507000;
            break;
        case 7:
            saleprice.amount = 57000;
            break;
        case 8:
            saleprice.amount = 47000;
            break;
        case 9:
            saleprice.amount = 37000;
            break;
        case 10:
            saleprice.amount = 27000;
            break;
        case 11:
            saleprice.amount = 17000;
            break;
        case 12:
            saleprice.amount = 8000;
            break;
        default:
            eosio_assert(1==0, "Custom Error: Incorrect custom name length");
            break;
        }

    }

    // Check the correct amount has been transferred
    eosio_assert(quantity == saleprice, "Custom Error: Wrong amount transferred.");

    // Stats table index
    uint64_t index = 0;
    if (suffix == ".e") 
    {
        index = 1;
    }
    else if (suffix == ".x") 
    {
        index = 2;
    }
    else if (suffix == ".y") 
    {
        index = 3;
    }
    else if (suffix == ".z") 
    {
        index = 4;
    }
    
    // Update stats table
    _stats.modify(_stats.find(index), _self, [&](auto &s) {
        s.num_purchased++;
        s.tot_sales += saleprice;
    });

    // Account to transfer fees to + memo
    name suffix_owner;
    string memo;

    if (suffix == ".x") {

        suffix_owner = name("buyname.x");
        memo = account_name.to_string() + "-" + owner_key + "-nameswapsfee"; 
    
    } else if (suffix == ".y" || suffix == ".z") {

        suffix_owner = name("buyname.x");
        memo = account_name.to_string() + "-" + owner_key;

    } else if (suffix == ".e") {

        suffix_owner = name("e");
        memo = account_name.to_string() + "+" + owner_key + "+219959";

    }

    // Transfer funds to suffix owner
    action(
        permission_level{_self, name("active")},
        name("eosio.token"), name("transfer"),
        std::make_tuple(_self, suffix_owner, saleprice, memo))
        .send();
}

void eosnameswaps::buy_saleprice(const name account_to_buy, const name from, const asset quantity, const string owner_key, const string active_key, const string referrer)
{

    // ----------------------------------------------
    // Sale/Bid price
    // ----------------------------------------------

    // Check the account is available to buy
    auto itr_accounts = _accounts.find(account_to_buy.value);
    eosio_assert(itr_accounts != _accounts.end(), (std::string("Buy Error: Account ") + account_to_buy.to_string() + std::string(" is not for sale.")).c_str());

    // Sale price
    auto saleprice = itr_accounts->saleprice;

    // Check the correct amount of EOS was transferred
    if (quantity != saleprice)
    {
        auto itr_bids = _bids.find(account_to_buy.value);

        // Current bid decision by seller
        const int bid_decision = itr_bids->bidaccepted;

        if (quantity == itr_bids->bidprice)
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
            eosio_assert(itr_bids->bidder == from, "Buy Error: Only the accepted bidder can purchase the account at the bid price.");

            // Lower sale price to the bid price for the bidder only
            saleprice = itr_bids->bidprice;
        }
    }

    eosio_assert(saleprice == quantity, "Buy Error: You have not transferred the correct amount of EOS. Check the sale price.");

    // ----------------------------------------------
    // Seller, Contract, & Referrer fees
    // ----------------------------------------------

    auto sellerfee = asset(0, symbol("EOS", 4));
    auto contractfee = asset(0, symbol("EOS", 4));

    // Fee amounts
    contractfee.amount = int(saleprice.amount * contract_pc);
    sellerfee.amount = saleprice.amount - contractfee.amount;

    // Look up the referrer account
    if (referrer.length() > 0)
    {
        auto itr_referrer = _referrer.find(name(referrer).value);
        if (itr_referrer != _referrer.end())
        {

            auto referrerfee = asset(int(referrer_pc * contractfee.amount), symbol("EOS", 4));
            contractfee.amount -= referrerfee.amount;

            // Transfer EOS from contract to referrer fees account
            action(
                permission_level{_self, name("active")},
                name("eosio.token"), name("transfer"),
                std::make_tuple(_self, itr_referrer->ref_account, referrerfee, std::string("EOSNameSwaps: Account referrer fee: ") + itr_accounts->account4sale.to_string()))
                .send();
        }
    }

    // Transfer EOS from contract to contract fees account
    action(
        permission_level{_self, name("active")},
        name("eosio.token"), name("transfer"),
        std::make_tuple(_self, feesaccount, contractfee, std::string("EOSNameSwaps: Account contract fee: ") + itr_accounts->account4sale.to_string()))
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
    account_auth(itr_accounts->account4sale, from, name("active"), name("owner"), active_key);

    // Remove seller@active permissions and replace with buyer@owner account and the supplied key
    account_auth(itr_accounts->account4sale, from, name("owner"), name(""), owner_key);

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
    send_message(from, string("EOSNameSwaps: You have successfully bought the account ") + name{account_to_buy}.to_string() + string(". Please come again."));
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

// Action: Register Referrer
void eosnameswaps::regref(const regref_type &regref_data)
{

    // ----------------------------------------------
    // Auth checks
    // ----------------------------------------------

    eosio_assert(has_auth(_self), "Referrer Error: Only the contract account can register referrers.");

    // ----------------------------------------------
    // Valid transaction checks
    // ----------------------------------------------

    // Check the referrer account exists
    eosio_assert(is_account(regref_data.ref_account), "Referrer Error: The referrer account does not exist.");

    // ----------------------------------------------
    // Update table
    // ----------------------------------------------

    // Place data in referrer table. Referrer pays for ram storage
    _referrer.emplace(_self, [&](auto &s) {
        s.ref_name = regref_data.ref_name;
        s.ref_account = regref_data.ref_account;
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

    // Only the contract account can perform screening
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

// Init the stats table
void eosnameswaps::initstats() {

    // ----------------------------------------------
    // Auth checks
    // ----------------------------------------------

    // Only the contract account can init the stats table
    require_auth(_self);

    // ----------------------------------------------

    // Init stats table
    auto itr_stats = _stats.find(0);
    if (itr_stats == _stats.end())
    {
    
        for (int index = 0; index <= 4; index++) {        
            _stats.emplace(_self, [&](auto &s) {
                s.index = index;
                s.num_listed = 0;
                s.num_purchased = 0;
                s.tot_sales = asset(0, symbol("EOS", 4));
                s.tot_fees = asset(0, symbol("EOS", 4));
            });
        }
    
    }
    
    
}

// Broadcast message
void eosnameswaps::send_message(name receiver, string message)
{

    action(permission_level{_self, name("active")},
           name("eosnameswaps"), name("message"),
           std::make_tuple(
               receiver,
               message))
        .send();
}

// Changes the owner/active permissions
void eosnameswaps::account_auth(name account4sale, name changeto, name perm_child, name perm_parent, string pubkey_str)
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
    require_auth2(_self.value, name("initloan").value);

    // ----------------------------------------------
    // Valid transaction checks
    // ----------------------------------------------

    // Define the delegated bandwidth table
    typedef eosio::multi_index<name("delband"), delegated_bandwidth> db_table;

    // Get references to the contract loan accounts db tables
    db_table db_nameswapsln1(name("eosio"), name("nameswapsln1").value);
    db_table db_nameswapsln2(name("eosio"), name("nameswapsln2").value);
    db_table db_nameswapsln3(name("eosio"), name("nameswapsln3").value);
    db_table db_nameswapsln4(name("eosio"), name("nameswapsln4").value);

    // Check the contract hasnt loaned to this account in the last 12 hours
    eosio_assert(db_nameswapsln1.find(lend_data.account4sale.value) == db_nameswapsln1.end(), "Lend Error: You can only recieve a loan once in 12 hours.");
    eosio_assert(db_nameswapsln2.find(lend_data.account4sale.value) == db_nameswapsln2.end(), "Lend Error: You can only recieve a loan once in 12 hours.");
    eosio_assert(db_nameswapsln3.find(lend_data.account4sale.value) == db_nameswapsln3.end(), "Lend Error: You can only recieve a loan once in 12 hours.");
    eosio_assert(db_nameswapsln4.find(lend_data.account4sale.value) == db_nameswapsln4.end(), "Lend Error: You can only recieve a loan once in 12 hours.");

    // Limit lending to 5/1 EOS for CPU/NET
    eosio_assert(lend_data.cpu.amount <= 50000 && lend_data.net.amount <= 10000, "Lend Error: Max loan of 5 EOS for CPU and 1 EOS for NET.");

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

    // ----------------------------------------------
    // Unlend bandwidth (deferred)
    // ----------------------------------------------

    transaction t{};

    t.actions.emplace_back(
        permission_level{loan_account, name("loaner")},
        name("eosio"), name("undelegatebw"),
        std::make_tuple(loan_account, lend_data.account4sale, lend_data.net, lend_data.cpu));

    // Wait 12hr before unstaking
    t.delay_sec = 12 * 3600;
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
        else if (code == receiver && action == name("lend").value)
        {
            execute_action(name(receiver), name(code), &eosnameswaps::lend);
        }
        else if (code == receiver && action == name("regref").value)
        {
            execute_action(name(receiver), name(code), &eosnameswaps::regref);
        }
        else if (code == receiver && action == name("initstats").value)
        {
            execute_action(name(receiver), name(code), &eosnameswaps::initstats);
        }
        eosio_exit(0);
    }
}

} // namespace eosio
