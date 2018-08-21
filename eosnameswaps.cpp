/**
 *  @file
 *  @copyright defined in eos/LICENSE.txt
 */

#include <string>
#include <vector>
#include <iostream>

#include <eosiolib/asset.hpp>
#include <eosiolib/eosio.hpp>
#include <eosiolib/contract.hpp>

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

    // Change auth from account4sale@active to contract@active
    // This ensures eosio.code permission has been set to the contract
    account_auth(sell_data.account4sale, _self, N(active), N(owner));

    // Change auth from contract@owner to owner@owner
    // This ensures the contract is the only owner
    account_auth(sell_data.account4sale, _self, N(owner), N());

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
    // This is necessary to allow transfers of EOS to the contract.
    if (transfer_data.from == _self || transfer_data.from == N(namedaccount))
    {
        return;
    }

    // ----------------------------------------------
    // Valid transaction checks
    // ----------------------------------------------

    // Check the transfer is valid
    eosio_assert(transfer_data.quantity.symbol == string_to_symbol(4, "EOS"), "Invalid payment: You must pay in EOS.");
    eosio_assert(transfer_data.quantity.is_valid(), "Buy Error: Quantity is not valid.");
    eosio_assert(transfer_data.quantity >= asset(10000), "Buy Error: Minimum price is 1.0000 EOS.");

    // Check the buy memo is valid
    eosio_assert(transfer_data.memo.length() <= 12, "Buy Error: Transfer memo must contain an account name you wish to buy.");

    // Extract account to buy from memo
    const string account_string = transfer_data.memo.substr(0, 12);
    const account_name account_to_buy = string_to_name(account_string.c_str());

    // Check an account name has been listed in the memo
    eosio_assert(account_to_buy != N(), "Buy Error: You must specify an account name to buy in the memo.");
    eosio_assert(account_to_buy != _self, "Buy Error: You cannot buy/sell/cancel the contract!.");

    // Check the account is available to buy
    auto itr = _accounts.find(account_to_buy);
    eosio_assert(itr != _accounts.end(), std::string("Buy Error: Account " + name{account_to_buy}.to_string() + " is not for sale.").c_str());

    // Check the correct amount of EOS was transferred
    eosio_assert(itr->saleprice.amount == transfer_data.quantity.amount, "Buy Error: You have not transferred the correct amount of EOS. Check the sale price.");

    // ----------------------------------------------
    // Seller and contract fees
    // ----------------------------------------------

    // Sale price
    auto saleprice = itr->saleprice;

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
        std::make_tuple(_self, get_contractfees(), contractfee, std::string("EOSNAMESWAPS: Account sale contract fee: " + name{itr->account4sale}.to_string())))
        .send();

    // Transfer EOS from contract to seller minus the contract fee
    action(
        permission_level{_self, N(owner)},
        N(eosio.token), N(transfer),
        std::make_tuple(_self, itr->paymentaccnt, sellerfee, std::string("EOSNAMESWAPS: Account sale fee: " + name{itr->account4sale}.to_string())))
        .send();

    // ----------------------------------------------
    // Update account owner
    // ----------------------------------------------

    // Remove contract@owner permissions and replace with buyer@owner account.
    account_auth(itr->account4sale, transfer_data.from, N(active), N(owner));

    // Remove seller@active permissions and replace with buyer@owner account.
    account_auth(itr->account4sale, transfer_data.from, N(owner), N());

    // Erase account from table
    _accounts.erase(itr);

}

// Action: Remove a listed account from sale
void eosnameswaps::cancel(const cancel_type &cancel_data)
{

    // ----------------------------------------------
    // Cancel data checks
    // ----------------------------------------------

    // Check an account with that name is listed for sale
    auto itr = _accounts.find(cancel_data.account4sale);
    eosio_assert(itr != _accounts.end(), "Cancel Error: That account name is not listed for sale");

    // Only the paymentaccnt can cancel the sale (the contract has the owner key)
    require_auth(itr->paymentaccnt);

    // Basic checks (redundant?)
    eosio_assert(cancel_data.account4sale != N(), "Cancel Error: You must specify an account name to cancel.");
    eosio_assert(cancel_data.account4sale != _self, "Cancel Error: You cannot buy/sell/cancel the contract!.");

    // ----------------------------------------------
    // Update account owners
    // ----------------------------------------------

    // Change auth from contract@active to paymentaccnt@owner
    account_auth(cancel_data.account4sale, itr->paymentaccnt, N(active), N(owner));

    // Change auth from contract@owner to paymentaccnt@owner
    account_auth(cancel_data.account4sale, itr->paymentaccnt, N(owner), N());

    // ----------------------------------------------
    // Cleanup
    // ----------------------------------------------

    // Erase account from table
    _accounts.erase(itr);
}


void eosnameswaps::account_auth(account_name account4sale, account_name changeto, permission_name perm_child, permission_name perm_parent)
{

    // Key to take over permission
    eosiosystem::key_weight key_weight_buyer{
        .key = eosio::public_key{},
        .weight = (uint16_t)1};

    // Account to take over permission changeto@perm_child
    eosiosystem::permission_level_weight accounts{
        .permission = permission_level{changeto, perm_child},
        .weight = (uint16_t)1,
    };

    // Setup authority for contract. Choose either a new key, or account, or both.
    authority contract_authority{
        .threshold = 1,
        .keys = {},
        .accounts = {accounts},
        .waits = {}};

    // Remove contract permissions and replace with changeto account.
    action(permission_level{account4sale, N(owner)},
           N(eosio), N(updateauth),
           std::make_tuple(
               account4sale,
               perm_child,
               perm_parent,
               contract_authority))
        .send();
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
        default:
            break;
        }
        eosio_exit(0);
    }
}

} // namespace eosio
