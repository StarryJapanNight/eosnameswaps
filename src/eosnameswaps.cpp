/**
 *  @file
 *  @copyright defined in eos/LICENSE.txt
 */

#include "../include/eosnameswaps.hpp"

namespace eosio
{

// Action: Sell an account
void eosnameswaps::sell(name account4sale,
                        asset saleprice,
                        name paymentaccnt,
                        string message)
{

    // ----------------------------------------------
    // Auth checks
    // ----------------------------------------------

    // Only the account4sale@owner can sell (contract@eosio.code must already be an owner)
    require_auth(permission_level{account4sale, name("owner")});

    // ----------------------------------------------
    // Valid transaction checks
    // ----------------------------------------------

    // Check an account with that name is not already listed for sale
    auto itr_accounts = _accounts.find(account4sale.value);
    check(itr_accounts == _accounts.end(), "That account is already for sale.");

    // Check the payment account exists
    check(is_account(paymentaccnt), "Sell Error: The payment account does not exist.");

    // Check the payment account is not the account4sale
    check(paymentaccnt != account4sale, "Sell Error: The payment account cannot be the account for sale!");

    // Check the transfer is valid
    check(saleprice.symbol == network_symbol, (string("Sell Error: Sale price must be in ") + symbol_name + string(". Ex: 10.0000 ") + symbol_name + string(".")).c_str());
    check(saleprice.is_valid(), "Sell Error: Sale price is not valid.");
    check(saleprice >= asset(10000, network_symbol), (string("Sell Error: Sale price must be at least 1 ") + symbol_name + string(". Ex: 1.0000 ") + symbol_name + string(".")).c_str());

    // Check the message is not longer than 100 characters
    check(message.length() <= 100, "Sell Error: The message must be <= 100 characters.");

    // Invalidate any past MSIGs
    action(
        permission_level{account4sale, name("owner")},
        name("eosio.msig"), name("invalidate"),
        std::make_tuple(account4sale.value))
        .send();

    // ----------------------------------------------
    // Change account ownership
    // ----------------------------------------------

    // Change auth from account4sale@active to contract@active
    // This ensures eosio.code permission has been set to the contract
    account_auth(account4sale, _self, name("active"), name("owner"), "None");

    // Change auth from contract@owner to owner@owner
    // This ensures the contract is the only owner
    account_auth(account4sale, _self, name("owner"), name(""), "None");

    // ----------------------------------------------
    // Add data to tables
    // ----------------------------------------------

    // Place data in accounts table. Seller pays for ram storage
    _accounts.emplace(account4sale, [&](auto &s) {
        s.account4sale = account4sale;
        s.saleprice = saleprice;
        s.paymentaccnt = paymentaccnt;
    });

    // Place data in extras table. Seller pays for ram storage
    _extras.emplace(account4sale, [&](auto &s) {
        s.account4sale = account4sale;
        s.screened = false;
        s.numberofvotes = 0;
        s.last_voter = name("");
        s.message = message;
    });

    // Place data in bids table. Bidder pays for ram storage
    _bids.emplace(account4sale, [&](auto &s) {
        s.account4sale = account4sale;
        s.bidaccepted = 1;
        s.bidprice = asset(0, network_symbol);
        s.bidder = name("");
    });

    // Place data in stats table. Contract pays for ram storage
    auto itr_stats = _stats.find(0);
    _stats.modify(itr_stats, _self, [&](auto &s) {
        s.num_listed++;
    });

    // Send message
    send_message(paymentaccnt, string("EOSNameSwaps: Your account ") + name{account4sale}.to_string() + string(" has been listed for sale. Keep an eye out for bids, and don't forget to vote for accounts you like!"));
}

// Action: Buy an account listed for sale
void eosnameswaps::buy(name from,
                       name to,
                       asset quantity,
                       string memo)
{

    // ----------------------------------------------
    // Auth checks
    // ----------------------------------------------

    // Important: The transfer fees actions below will trigger this function without this
    if (from == _self)
        return;

    // EOSBet hack
    check(to == _self, "Buy Error: Transfer must be direct to contract.");

    // ----------------------------------------------
    // Valid transaction checks
    // ----------------------------------------------

    // Check the buy code is valid
    const string buy_code = memo.substr(0, 3);
    check(buy_code == "cn:" || buy_code == "sp:" || buy_code == "mk:", "Buy Error: Malformed buy string.");

    // Check the transfer is valid
    check(quantity.symbol == network_symbol, (string("Buy Error: You must pay in ") + symbol_name + string(".")).c_str());
    check(quantity.is_valid(), "Buy Error: Quantity is not valid.");

    // ----------------------------------------------

    // Strip buy code from memo
    const string memo2 = memo.substr(3);

    // Find the length of the account name
    int name_length = 0;
    for (int lp = 1; lp <= 12; ++lp)
    {
        if (memo2[lp] == ',')
        {
            name_length = lp;
            break;
        }
    }

    // Check the name length is valid
    check(name_length > 0, "Buy Error: Malformed buy name.");

    // Extract account to buy from memo
    const name account_name = name(memo2.substr(0, name_length));

    // Extract keys
    const string owner_key = memo2.substr(name_length + 1, KEY_LENGTH);
    const string active_key = memo2.substr(name_length + 2 + KEY_LENGTH, KEY_LENGTH);

    string referrer = "";
    if (memo2.length() > name_length + 3 + 2 * KEY_LENGTH && memo2.length() <= name_length + 3 + 2 * KEY_LENGTH + 12)
    {
        referrer = memo2.substr(name_length + 3 + 2 * KEY_LENGTH);
    }

    // Call the required function
    if (buy_code == "cn:")
    {
        buy_custom(account_name, from, quantity, owner_key, active_key);
    }
    else if (buy_code == "sp:")
    {
        buy_saleprice(account_name, from, quantity, owner_key, active_key, referrer);
    }
    else if (buy_code == "mk:")
    {
        make_account(account_name, from, quantity, owner_key, active_key);
    }
}

void eosnameswaps::buy_custom(const name account_name, const name from, const asset quantity, const string owner_key, const string active_key)
{

    // Account name length
    int name_length = account_name.length();

    // Extract suffix
    string suffix = (account_name).to_string().substr(name_length - 2, 2);

    // Currently supported suffixes
    check(suffix == ".e" || suffix == ".x" || suffix == ".y" || suffix == ".z", "Custom Error: That is not a valid suffix.");

    // Custom name saleprice
    asset saleprice = asset(0, network_symbol);

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
            check(1 == 0, "Custom Error: Incorrect custom name length");
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
            check(1 == 0, "Custom Error: Incorrect custom name length");
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
            check(1 == 0, "Custom Error: Incorrect custom name length");
            break;
        }
    }

    // Check the correct amount has been transferred
    check(quantity == saleprice, "Custom Error: Wrong amount transferred.");

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

    if (suffix == ".x" || suffix == ".y" || suffix == ".z")
    {

        suffix_owner = name("buyname.x");
        memo = account_name.to_string() + "-" + owner_key + "-nameswapsfee";
    }
    else if (suffix == ".e")
    {

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

void eosnameswaps::make_account(const name account_name, const name from, const asset quantity, const string owner_key_str, const string active_key_str)
{

    // ----------------------------------------------
    // Valid transaction checks
    // ----------------------------------------------

    // Check the correct amount has been transferred
    check(quantity == newaccountfee, "Custom Error: Wrong amount transferred.");

    // ----------------------------------------------

    authority owner_auth = keystring_authority(owner_key_str);
    authority active_auth = keystring_authority(active_key_str);

    // Create account
    action(
        permission_level{_self, name("active")},
        name("eosio"), name("newaccount"),
        std::make_tuple(_self, account_name, owner_auth, active_auth))
        .send();

    // Buy ram
    action(
        permission_level{_self, name("active")},
        name("eosio"), name("buyram"),
        std::make_tuple(_self, account_name, newaccountram))
        .send();

    // Delegate CPU/NET
    action(
        permission_level{_self, name("active")},
        name("eosio"), name("delegatebw"),
        std::make_tuple(_self, account_name, newaccountnet, newaccountcpu, 1))
        .send();

    // Stats table index
    uint64_t index = 5;

    // Update stats table
    _stats.modify(_stats.find(index), _self, [&](auto &s) {
        s.num_purchased++;
        s.tot_sales += newaccountfee;
    });
}

authority eosnameswaps::keystring_authority(string key_str)
{

    // Convert string to key type
    const abieos::public_key key = abieos::string_to_public_key(key_str);

    // Setup authority
    authority ret_authority;

    // Array to hold public key
    std::array<char, 33> key_char;

    // Copy key to char array
    std::copy(key.data.begin(), key.data.end(), key_char.begin());

    key_weight kweight{
        .key = {(uint8_t)key_type::k1, key_char},
        .weight = (uint16_t)1};

    // Authority
    ret_authority.threshold = 1;
    ret_authority.keys = {kweight};
    ret_authority.accounts = {};
    ret_authority.waits = {};

    return ret_authority;
}

void eosnameswaps::buy_saleprice(const name account_to_buy, const name from, const asset quantity, const string owner_key, const string active_key, const string referrer)
{

    // ----------------------------------------------
    // Sale/Bid price
    // ----------------------------------------------

    // Check the account is available to buy
    auto itr_accounts = _accounts.find(account_to_buy.value);
    check(itr_accounts != _accounts.end(), (string("Buy Error: Account ") + account_to_buy.to_string() + string(" is not for sale.")).c_str());

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
                check(bid_decision != BID_REJECTED, "Buy Error: The bid has been rejected. Bid higher.");

                // Check the bid has been decided
                check(bid_decision != BID_UNDECIDED, "Buy Error: The bid has not been accepted or rejected yet.");
            }

            // Check the bid is from the accepted bidder
            check(itr_bids->bidder == from, "Buy Error: Only the accepted bidder can purchase the account at the bid price.");

            // Lower sale price to the bid price for the bidder only
            saleprice = itr_bids->bidprice;
        }
    }

    check(saleprice == quantity, (string("Buy Error: You have not transferred the correct amount of ") + symbol_name + string(". Check the sale price.")).c_str());

    // ----------------------------------------------
    // Seller, Contract, & Referrer fees
    // ----------------------------------------------

    auto sellerfee = asset(0, network_symbol);
    auto contractfee = asset(0, network_symbol);

    // Fee amounts
    contractfee.amount = int(saleprice.amount * contract_pc);
    sellerfee.amount = saleprice.amount - contractfee.amount;

    // Look up the referrer account
    if (referrer.length() > 0)
    {
        auto itr_referrer = _referrer.find(name(referrer).value);
        if (itr_referrer != _referrer.end())
        {

            auto referrerfee = asset(int(referrer_pc * contractfee.amount), network_symbol);
            contractfee.amount -= referrerfee.amount;

            // Transfer EOS from contract to referrer fees account
            action(
                permission_level{_self, name("active")},
                name("eosio.token"), name("transfer"),
                std::make_tuple(_self, itr_referrer->ref_account, referrerfee, string("EOSNameSwaps: Account referrer fee: ") + itr_accounts->account4sale.to_string()))
                .send();
        }
    }

    // Transfer EOS from contract to contract fees account
    action(
        permission_level{_self, name("active")},
        name("eosio.token"), name("transfer"),
        std::make_tuple(_self, feesaccount, contractfee, string("EOSNameSwaps: Account contract fee: ") + itr_accounts->account4sale.to_string()))
        .send();

    // Transfer EOS from contract to seller minus the contract fees
    action(
        permission_level{_self, name("active")},
        name("eosio.token"), name("transfer"),
        std::make_tuple(_self, itr_accounts->paymentaccnt, sellerfee, string("EOSNameSwaps: Account seller fee: ") + itr_accounts->account4sale.to_string()))
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
void eosnameswaps::cancel(name account4sale,
                          string owner_key_str,
                          string active_key_str)
{

    // ----------------------------------------------
    // Auth checks
    // ----------------------------------------------

    // Check an account with that name is listed for sale
    auto itr_accounts = _accounts.find(account4sale.value);
    check(itr_accounts != _accounts.end(), "Cancel Error: That account name is not listed for sale");

    // Only the payment account can cancel the sale (the contract has the owner key)
    check(has_auth(itr_accounts->paymentaccnt) || has_auth(_self), "Cancel Error: Only the payment account can cancel the sale.");

    // ----------------------------------------------
    // Update account owners
    // ----------------------------------------------

    // Change auth from contract@active to submitted active key
    account_auth(account4sale, itr_accounts->paymentaccnt, name("active"), name("owner"), active_key_str);

    // Change auth from contract@owner to submitted owner key
    account_auth(account4sale, itr_accounts->paymentaccnt, name("owner"), name(""), owner_key_str);

    // ----------------------------------------------
    // Cleanup
    // ----------------------------------------------

    // Erase account from accounts table
    _accounts.erase(itr_accounts);

    // Erase account from the extras table
    auto itr_extras = _extras.find(account4sale.value);
    _extras.erase(itr_extras);

    // Erase account from the bids table
    auto itr_bids = _bids.find(account4sale.value);
    _bids.erase(itr_bids);

    // Place data in stats table. Contract pays for ram storage
    auto itr_stats = _stats.find(0);
    _stats.modify(itr_stats, _self, [&](auto &s) {
        s.num_listed--;
    });

    // Send message
    send_message(itr_accounts->paymentaccnt, string("EOSNameSwaps: You have successfully cancelled the sale of the account ") + name{account4sale}.to_string() + string(". Please come again."));
}

// Action: Remove a listed account from sale
void eosnameswaps::remove(name account4sale)
{

    // ----------------------------------------------
    // Auth checks
    // ----------------------------------------------

    // Check an account with that name is listed for sale
    auto itr_accounts = _accounts.find(account4sale.value);
    check(itr_accounts != _accounts.end(), "Cancel Error: That account name is not listed for sale");

    // Only the contract account can remove the sale (the contract has the owner key)
    check(has_auth(_self), "Cancel Error: Only the contract account can remove the sale.");

    // Cleanup
    // ----------------------------------------------

    // Erase account from accounts table
    _accounts.erase(itr_accounts);

    // Erase account from the extras table
    auto itr_extras = _extras.find(account4sale.value);
    _extras.erase(itr_extras);

    // Erase account from the bids table
    auto itr_bids = _bids.find(account4sale.value);
    _bids.erase(itr_bids);
}

// Action: Update the sale price
void eosnameswaps::update(name account4sale,
                          asset saleprice,
                          string message)
{

    // ----------------------------------------------
    // Auth checks
    // ----------------------------------------------

    // Check an account with that name is listed for sale
    auto itr_accounts = _accounts.find(account4sale.value);
    check(itr_accounts != _accounts.end(), "Update Error: That account name is not listed for sale");

    // Only the payment account can update the sale price
    check(has_auth(itr_accounts->paymentaccnt), "Update Error: Only the payment account can update a sale.");

    // ----------------------------------------------
    // Valid transaction checks
    // ----------------------------------------------

    // Check the transfer is valid
    check(saleprice.symbol == network_symbol, (string("Update Error: Sale price must be in ") + symbol_name + string(". Ex: 10.0000 ") + symbol_name + string(".")).c_str());
    check(saleprice.is_valid(), "Update Error: Sale price is not valid.");
    check(saleprice >= asset(10000, network_symbol), (string("Update Error: Sale price must be at least 1 ") + symbol_name + string(". Ex: 1.0000 ") + symbol_name + string(".")).c_str());

    // Check the message is not longer than 100 characters
    check(message.length() <= 100, "Sell Error: The message must be <= 100 characters.");

    // ----------------------------------------------
    // Update tables
    // ----------------------------------------------

    // Place data in accounts table. Payment account pays for ram storage
    _accounts.modify(itr_accounts, itr_accounts->paymentaccnt, [&](auto &s) {
        s.saleprice = saleprice;
    });

    // Place data in extras table. Payment account pays for ram storage
    auto itr_extras = _extras.find(account4sale.value);
    _extras.modify(itr_extras, itr_accounts->paymentaccnt, [&](auto &s) {
        s.message = message;
    });

    // Send message
    send_message(itr_accounts->paymentaccnt, string("EOSNameSwaps: You have successfully updated the sale of the account ") + name{account4sale}.to_string());
}

// Action: Increment votes
void eosnameswaps::vote(name account4sale,
                        name voter)
{

    // ----------------------------------------------
    // Auth checks
    // ----------------------------------------------

    // Confirm the voter is who they say they are
    check(has_auth(voter), "Vote Error: You are not who you say you are. Check permissions.");

    // ----------------------------------------------
    // Valid transaction checks
    // ----------------------------------------------

    // Check an account with that name is listed for sale
    auto itr_extras = _extras.find(account4sale.value);
    check(itr_extras != _extras.end(), "Vote Error: That account name is not listed for sale.");

    // Can only vote once in a row
    check(voter != itr_extras->last_voter, "Vote Error: You have already voted for this account!");

    // ----------------------------------------------
    // Update table
    // ----------------------------------------------

    // Place data in extras table. Voter pays for ram storage
    _extras.modify(itr_extras, voter, [&](auto &s) {
        s.numberofvotes++;
        s.last_voter = voter;
    });
}

// Action: Register Referrer
void eosnameswaps::regref(eosio::name ref_name,
                          eosio::name ref_account)
{

    // ----------------------------------------------
    // Auth checks
    // ----------------------------------------------

    check(has_auth(_self), "Referrer Error: Only the contract account can register referrers.");

    // ----------------------------------------------
    // Valid transaction checks
    // ----------------------------------------------

    // Check the referrer account exists
    check(is_account(ref_account), "Referrer Error: The referrer account does not exist.");

    // ----------------------------------------------
    // Update table
    // ----------------------------------------------

    // Place data in referrer table. Contract pays for ram storage
    _referrer.emplace(_self, [&](auto &s) {
        s.ref_name = ref_name;
        s.ref_account = ref_account;
    });
}

// Action: Register Shop
void eosnameswaps::regshop(name shopname,
                           string title,
                           string description,
                           name payment1,
                           name payment2,
                           name payment3)
{

    // ----------------------------------------------
    // Auth checks
    // ----------------------------------------------

    check(has_auth(_self), "Referrer Error: Only the contract account can register shops.");

    // ----------------------------------------------
    // Update table
    // ----------------------------------------------

    auto itr_shops = _shops.find(shopname.value);

    // Create new shop
    if (itr_shops == _shops.end())
    {

        // Place data in shoptable. Contract pays for ram storage
        _shops.emplace(_self, [&](auto &s) {
            s.shopname = shopname;
            s.title = title;
            s.description = description;
            s.payment1 = payment1;
            s.payment2 = payment2;
            s.payment3 = payment3;
        });
    }
    else // Modify shop
    {

        // Place data in shoptable. Contract pays for ram storage
        _shops.modify(itr_shops, _self, [&](auto &s) {
            s.shopname = shopname;
            s.title = title;
            s.description = description;
            s.payment1 = payment1;
            s.payment2 = payment2;
            s.payment3 = payment3;
        });
    }
}

// Action: Propose a bid for an account
void eosnameswaps::proposebid(name account4sale,
                              asset bidprice,
                              name bidder)
{

    // ----------------------------------------------
    // Auth checks
    // ----------------------------------------------

    // Confirm the bidder is who they say they are
    check(has_auth(bidder), "Propose Bid Error: You are not who you say you are. Check permissions.");

    // ----------------------------------------------
    // Valid transaction checks
    // ----------------------------------------------

    // Check an account with that name is listed for sale
    auto itr_bids = _bids.find(account4sale.value);
    check(itr_bids != _bids.end(), "Propose Bid Error: That account name is not listed for sale");

    // Check the transfer is valid
    check(bidprice.symbol == network_symbol, (string("Propose Bid Error: Bid price must be in ") + symbol_name + string(". Ex: 10.0000 ") + symbol_name + string(".")).c_str());
    check(bidprice.is_valid(), "Propose Bid Error: Bid price is not valid.");
    check(bidprice >= asset(10000, network_symbol), (string("Propose Bid Error: The minimum bid price is 1.0000 ") + symbol_name + string(".")).c_str());

    // Only accept new bids if they are higher
    check(bidprice > itr_bids->bidprice, "Propose Bid Error: You must bid higher than the last bidder.");

    // Only accept new bids if they are lower than the sale price
    auto itr_accounts = _accounts.find(account4sale.value);
    check(bidprice <= itr_accounts->saleprice, "Propose Bid Error: You must bid lower than the sale price.");

    // ----------------------------------------------
    // Update table
    // ----------------------------------------------

    // Place data in bids table. Bidder pays for ram storage
    _bids.modify(itr_bids, bidder, [&](auto &s) {
        s.bidaccepted = 1;
        s.bidprice = bidprice;
        s.bidder = bidder;
    });

    // Send message
    send_message(itr_accounts->paymentaccnt, string("EOSNameSwaps: Your account ") + name{account4sale}.to_string() + string(" has received a bid. If you choose to accept it, the bidder can purchase the account at the lower price. Others can still bid higher or pay the full sale price until then."));
}

// Action: Accept or decline a bid for an account
void eosnameswaps::decidebid(name account4sale,
                             bool accept)
{

    // ----------------------------------------------
    // Auth checks
    // ----------------------------------------------

    // Check an account with that name is listed for sale
    auto itr_accounts = _accounts.find(account4sale.value);
    check(itr_accounts != _accounts.end(), "Decide Bid Error: That account name is not listed for sale.");

    // Only the payment account can accept bids
    check(has_auth(itr_accounts->paymentaccnt), "Decide Bid Error: Only the payment account can decide on bids.");

    // ----------------------------------------------
    // Valid transaction checks
    // ----------------------------------------------

    auto itr_bids = _bids.find(account4sale.value);

    // Check there is a bid to accept or reject
    check(itr_bids->bidprice != asset(0, network_symbol), "Decide Bid Error: There are no bids to accept or reject.");

    // ----------------------------------------------
    // Update table
    // ----------------------------------------------

    // Place data in bids table. Bidder pays for ram storage
    if (accept == true)
    {

        // Bid accepted
        _bids.modify(itr_bids, itr_accounts->paymentaccnt, [&](auto &s) {
            s.bidaccepted = 2;
        });

        // Send message
        send_message(itr_bids->bidder, string("EOSNameSwaps: Your bid for ") + name{account4sale}.to_string() + string(" has been accepted. Account ") + name{itr_bids->bidder}.to_string() + string(" can buy it for the bid price. Be quick, as others can still outbid you or pay the full sale price."));
    }
    else
    {

        // Bid rejected
        _bids.modify(itr_bids, itr_accounts->paymentaccnt, [&](auto &s) {
            s.bidaccepted = 0;
        });

        // Send message
        send_message(itr_bids->bidder, string("EOSNameSwaps: Your bid for ") + name{account4sale}.to_string() + string(" has been rejected. Increase your bid offer"));
    }
}

// Null Action
void eosnameswaps::null()
{

    // ----------------------------------------------
    // Auth checks
    // ----------------------------------------------

    // Only the contract can send a message
    check(has_auth(_self), "Message Error: Only the contract can call the null action.");

    // ----------------------------------------------
}

// Message Action
void eosnameswaps::message(name receiver,
                           string message)
{

    // ----------------------------------------------
    // Auth checks
    // ----------------------------------------------

    // Only the contract can send a message
    check(has_auth(_self), "Message Error: Only the contract can send messages.");

    // ----------------------------------------------

    // Notify the specified account
    require_recipient(receiver);
}

// Action: Perform admin tasks
void eosnameswaps::screener(name account4sale,
                            uint8_t option)
{

    // ----------------------------------------------
    // Auth checks
    // ----------------------------------------------

    // Only the contract account can perform screening
    require_auth(_self);

    // ----------------------------------------------
    // Set the screening status of an account listed for sale
    // ----------------------------------------------

    int screened = option;

    check(screened >= 0 && screened <= 2, "Admin Error: Malformed screening data.");

    // Place data in table. Contract pays for ram storage
    auto itr_extras = _extras.find(account4sale.value);
    _extras.modify(itr_extras, _self, [&](auto &s) {
        s.screened = screened;
    });

    // ----------------------------------------------
}

// Init the stats table
void eosnameswaps::initstats()
{

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

        for (int index = 0; index <= 5; index++)
        {
            _stats.emplace(_self, [&](auto &s) {
                s.index = index;
                s.num_listed = 0;
                s.num_purchased = 0;
                s.tot_sales = asset(0, network_symbol);
                s.tot_fees = asset(0, network_symbol);
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
        contract_authority = keystring_authority(pubkey_str);
    }
    else
    {

        // Account to take over permission changeto@perm_child
        permission_level_weight accounts{
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

extern "C"
{
    void apply(uint64_t receiver, uint64_t code, uint64_t action)
    {

        if (code == name("eosio.token").value && action == name("transfer").value)
        {
            execute_action(name(receiver), name(code), &eosnameswaps::buy);
        }
        else if (code == receiver && action == name("null").value)
        {
            execute_action(name(receiver), name(code), &eosnameswaps::null);
        }
        else if (code == receiver && action == name("sell").value)
        {
            execute_action(name(receiver), name(code), &eosnameswaps::sell);
        }
        else if (code == receiver && action == name("cancel").value)
        {
            execute_action(name(receiver), name(code), &eosnameswaps::cancel);
        }
        else if (code == receiver && action == name("remove").value)
        {
            execute_action(name(receiver), name(code), &eosnameswaps::remove);
        }
        else if (code == receiver && action == name("update").value)
        {
            execute_action(name(receiver), name(code), &eosnameswaps::update);
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
        else if (code == receiver && action == name("regref").value)
        {
            execute_action(name(receiver), name(code), &eosnameswaps::regref);
        }
        else if (code == receiver && action == name("regshop").value)
        {
            execute_action(name(receiver), name(code), &eosnameswaps::regshop);
        }
        else if (code == receiver && action == name("initstats").value)
        {
            execute_action(name(receiver), name(code), &eosnameswaps::initstats);
        }
        eosio_exit(0);
    }
}

} // namespace eosio
