
HOST=http://127.0.0.1:8888
#HOST=http://jungle.cryptolions.io:18888

include ../test_keys.mk

# ---------------------------------------------------------------
# Setup
# ---------------------------------------------------------------
system_accounts: 

	# Make the system accounts
	cleos -u $(HOST) create account eosio eosio.bpay $(PUB_KEY_eosiobpay)
	cleos -u $(HOST) create account eosio eosio.msig $(PUB_KEY_eosiobpay)
	cleos -u $(HOST) create account eosio eosio.names $(PUB_KEY_eosiobpay)
	cleos -u $(HOST) create account eosio eosio.ram $(PUB_KEY_eosiobpay)
	cleos -u $(HOST) create account eosio eosio.ramfee $(PUB_KEY_eosiobpay)
	cleos -u $(HOST) create account eosio eosio.saving $(PUB_KEY_eosiobpay)
	cleos -u $(HOST) create account eosio eosio.stake $(PUB_KEY_eosiobpay)
	cleos -u $(HOST) create account eosio eosio.vpay $(PUB_KEY_eosiobpay)
	cleos -u $(HOST) create account eosio eosio.token $(PUB_KEY_eosiotoken)

deploy_token:
	# Deploy the eosio.token contract
	cleos -u $(HOST) set contract eosio.token /home/phil/eosio.contracts/build/eosio.token -p eosio.token@owner

deploy_msig:
	# Deploy System Contracts
	cleos -u $(HOST) set contract eosio.msig /home/phil/eosio.contracts/build/eosio.msig -p eosio.msig@owner

setup_token:
	# Create a EOS token
	cleos -u $(HOST) push action eosio.token create '["eosio", "1000000000.0000 EOS"]' -p eosio.token@owner
	cleos -u $(HOST) push action eosio.token issue '["eosio",  "500000000.0000 EOS", "issue"]' -p eosio

deploy_system:
	cleos -u $(HOST) set contract eosio /home/phil/eosio.contracts/build/eosio.system -p eosio
	cleos -u $(HOST) push action eosio init '[0,"4,EOS"]' -p eosio
	cleos -u $(HOST) system regproducer eosio $(PUB_KEY_eosio)

make_accounts:

	# Make the contract accounts
	cleos -u $(HOST) system newaccount eosio --stake-net "1000.0000 EOS" --stake-cpu "150000000.0000 EOS" --buy-ram-kbytes 1000 --transfer eosnameswaps $(PUB_KEY_eosnameswaps) 
	cleos -u $(HOST) system newaccount eosio --stake-net "100.0000 EOS" --stake-cpu "100.0000 EOS" --buy-ram-kbytes 1000 --transfer nameswapsfee $(PUB_KEY_nameswapsfee) 
	cleos -u $(HOST) system newaccount eosio --stake-net "100.0000 EOS" --stake-cpu "100.0000 EOS" --buy-ram-kbytes 1000 --transfer nameswapsln1 $(PUB_KEY_nameswapsln1) 
	cleos -u $(HOST) system newaccount eosio --stake-net "100.0000 EOS" --stake-cpu "100.0000 EOS" --buy-ram-kbytes 1000 --transfer nameswapsln2 $(PUB_KEY_nameswapsln2) 
	cleos -u $(HOST) system newaccount eosio --stake-net "100.0000 EOS" --stake-cpu "100.0000 EOS" --buy-ram-kbytes 1000 --transfer nameswapsln3 $(PUB_KEY_nameswapsln3) 
	cleos -u $(HOST) system newaccount eosio --stake-net "100.0000 EOS" --stake-cpu "100.0000 EOS" --buy-ram-kbytes 1000 --transfer nameswapsln4 $(PUB_KEY_nameswapsln4) 

	# Make the test acccounts
	cleos -u $(HOST) system newaccount eosio --stake-net "100.0000 EOS" --stake-cpu "100.0000 EOS" --buy-ram-kbytes 1000 --transfer eosnameswap1 $(PUB_KEY_eosnameswap1) 
	cleos -u $(HOST) system newaccount eosio --stake-net "100.0000 EOS" --stake-cpu "100.0000 EOS" --buy-ram-kbytes 1000 --transfer eosnameswap2 $(PUB_KEY_eosnameswap2)
	cleos -u $(HOST) system newaccount eosio --stake-net "100.0000 EOS" --stake-cpu "100.0000 EOS" --buy-ram-kbytes 1000 --transfer eosnameswap3 $(PUB_KEY_eosnameswap3)
	cleos -u $(HOST) system newaccount eosio --stake-net "100.0000 EOS" --stake-cpu "100.0000 EOS" --buy-ram-kbytes 1000 --transfer eosnameswap4 $(PUB_KEY_eosnameswap4)
	cleos -u $(HOST) system newaccount eosio --stake-net "100.0000 EOS" --stake-cpu "100.0000 EOS" --buy-ram-kbytes 1000 --transfer eosphilsmith $(PUB_KEY_eosphilsmith)
	cleos -u $(HOST) system newaccount eosio --stake-net "100.0000 EOS" --stake-cpu "100.0000 EOS" --buy-ram-kbytes 1000 --transfer hellodidieos $(PUB_KEY_hellodidieos)
	cleos -u $(HOST) system newaccount eosio --stake-net "100.0000 EOS" --stake-cpu "100.0000 EOS" --buy-ram-kbytes 1000 --transfer avocadocream $(PUB_KEY_avocadocream)
	cleos -u $(HOST) system newaccount eosio --stake-net "100.0000 EOS" --stake-cpu "100.0000 EOS" --buy-ram-kbytes 1000 --transfer wizardofozuk $(PUB_KEY_wizardofozuk)

issue_token:

	# Issue EOS tokens to the test accounts
	cleos -u $(HOST) push action eosio.token issue '["eosnameswaps","100.0000 EOS", "issue"]' -p eosio
	cleos -u $(HOST) push action eosio.token issue '["nameswapsfee","100.0000 EOS", "issue"]' -p eosio
	cleos -u $(HOST) push action eosio.token issue '["nameswapsln1","100.0000 EOS", "issue"]' -p eosio
	cleos -u $(HOST) push action eosio.token issue '["nameswapsln2","100.0000 EOS", "issue"]' -p eosio
	cleos -u $(HOST) push action eosio.token issue '["nameswapsln3","100.0000 EOS", "issue"]' -p eosio
	cleos -u $(HOST) push action eosio.token issue '["nameswapsln4","100.0000 EOS", "issue"]' -p eosio

	cleos -u $(HOST) push action eosio.token issue '["eosnameswap1","100.0000 EOS", "issue"]' -p eosio
	cleos -u $(HOST) push action eosio.token issue '["eosnameswap2","100.0000 EOS", "issue"]' -p eosio
	cleos -u $(HOST) push action eosio.token issue '["eosnameswap3","100.0000 EOS", "issue"]' -p eosio
	cleos -u $(HOST) push action eosio.token issue '["eosnameswap4","100.0000 EOS", "issue"]' -p eosio
	cleos -u $(HOST) push action eosio.token issue '["eosphilsmith","100.0000 EOS", "issue"]' -p eosio
	cleos -u $(HOST) push action eosio.token issue '["hellodidieos","100.0000 EOS", "issue"]' -p eosio
	cleos -u $(HOST) push action eosio.token issue '["avocadocream","100.0000 EOS", "issue"]' -p eosio
	cleos -u $(HOST) push action eosio.token issue '["wizardofozuk","100.0000 EOS", "issue"]' -p eosio

	# Vote for eosio as producer
	cleos -u $(HOST) system voteproducer prods eosnameswaps eosio

deploy_contract:
	# Deploy eosnameswaps Contract
	cleos -u $(HOST) set contract eosnameswaps /home/phil/eos-workspace/eosnameswaps -p eosnameswaps@owner

setup_contract:
	# Give contract permission to send actions
	cleos -u $(HOST) push action eosio updateauth '{"account":"eosnameswaps","permission":"owner","parent":"","auth":{"threshold": 1,"keys": [{"key": "$(PUB_KEY_eosnameswaps)","weight": 1}],"waits":[],"accounts": [{"permission":{"actor":"eosnameswaps","permission":"eosio.code"},"weight":1}]}}' -p eosnameswaps@owner
	cleos -u $(HOST) push action eosio updateauth '{"account":"eosnameswaps","permission":"active","parent":"owner","auth":{"threshold": 1,"keys": [{"key": "$(PUB_KEY_eosnameswaps)","weight": 1}],"waits":[],"accounts": [{"permission":{"actor":"eosnameswaps","permission":"eosio.code"},"weight":1}]}}' -p eosnameswaps@owner

    # Custom loaner permission
	cleos -u $(HOST) set account permission nameswapsln1 loaner '{"threshold":1,"keys":[{"key":"$(PUB_KEY_loaner)","weight":1}],"accounts": [{"permission":{"actor":"eosnameswaps","permission":"eosio.code"},"weight":1}]'} "active" -p nameswapsln1@active
	cleos -u $(HOST) set account permission nameswapsln2 loaner '{"threshold":1,"keys":[{"key":"$(PUB_KEY_loaner)","weight":1}],"accounts": [{"permission":{"actor":"eosnameswaps","permission":"eosio.code"},"weight":1}]'} "active" -p nameswapsln2@active
	cleos -u $(HOST) set account permission nameswapsln3 loaner '{"threshold":1,"keys":[{"key":"$(PUB_KEY_loaner)","weight":1}],"accounts": [{"permission":{"actor":"eosnameswaps","permission":"eosio.code"},"weight":1}]'} "active" -p nameswapsln3@active
	cleos -u $(HOST) set account permission nameswapsln4 loaner '{"threshold":1,"keys":[{"key":"$(PUB_KEY_loaner)","weight":1}],"accounts": [{"permission":{"actor":"eosnameswaps","permission":"eosio.code"},"weight":1}]'} "active" -p nameswapsln4@active

	# Loaner can delegatebw
	cleos -u $(HOST) set action permission nameswapsln1 eosio delegatebw loaner
	cleos -u $(HOST) set action permission nameswapsln2 eosio delegatebw loaner
	cleos -u $(HOST) set action permission nameswapsln3 eosio delegatebw loaner
	cleos -u $(HOST) set action permission nameswapsln4 eosio delegatebw loaner

	# Loaner can undelegatebw
	cleos -u $(HOST) set action permission nameswapsln1 eosio undelegatebw loaner
	cleos -u $(HOST) set action permission nameswapsln2 eosio undelegatebw loaner
	cleos -u $(HOST) set action permission nameswapsln3 eosio undelegatebw loaner
	cleos -u $(HOST) set action permission nameswapsln4 eosio undelegatebw loaner

	# Custom initloan permission
	cleos -u $(HOST) set account permission eosnameswaps initloan '{"threshold":1,"keys":[{"key":"$(PUB_KEY_initloan)","weight":1}]'} "active" -p eosnameswaps@active

	# Initloan can call lend function
	cleos -u $(HOST) set action permission eosnameswaps eosnameswaps lend initloan

# ---------------------------------------------------------------
# Setup Wallets
# ---------------------------------------------------------------

wallet_unlock:
	cleos wallet unlock -n main-wallet --password $(PRI_KEY_main_wallet)
	cleos wallet unlock -n test-wallet --password $(PRI_KEY_test_wallet)

wallet_import_test: 

	cleos wallet import --private-key $(PRI_KEY_nameswapsfee) -n test-wallet
	cleos wallet import --private-key $(PRI_KEY_nameswapsln1) -n test-wallet
	cleos wallet import --private-key $(PRI_KEY_nameswapsln2) -n test-wallet
	cleos wallet import --private-key $(PRI_KEY_nameswapsln3) -n test-wallet
	cleos wallet import --private-key $(PRI_KEY_nameswapsln4) -n test-wallet

	cleos wallet import --private-key $(PRI_KEY_eosio) -n test-wallet
	cleos wallet import --private-key $(PRI_KEY_eosiobpay) -n test-wallet
	cleos wallet import --private-key $(PRI_KEY_eosiotoken) -n test-wallet
	cleos wallet import --private-key $(PRI_KEY_everipediaiq) -n test-wallet

# ---------------------------------------------------------------
# Build Commands
# ---------------------------------------------------------------

build_cpp:
	eosio-cpp eosnameswaps.cpp -I /home/phil/eosio.contracts/ -o eosnameswaps.wasm 

get_tables:
	cleos -u $(HOST) get table eosnameswaps eosnameswaps accounts
	cleos -u $(HOST) get table eosnameswaps eosnameswaps extras
	cleos -u $(HOST) get table eosnameswaps eosnameswaps bids
	cleos -u $(HOST) get table eosnameswaps eosnameswaps stats

# ---------------------------------------------------------------
# Test Buy/Sell
# ---------------------------------------------------------------
test_sell:	

	# Change owner key to contracts active key
	cleos -u $(HOST) push action eosio updateauth '{"account":"eosnameswap1","permission":"owner","parent":"","auth":{"threshold": 1,"keys": [{"key":"$(PUB_KEY_eosnameswap1)","weight":1}],"waits":[],"accounts": [{"permission":{"actor":"eosnameswaps","permission":"eosio.code"},"weight":1}]}}' -p eosnameswap1@owner

	# Sell account
	cleos -u $(HOST) push action eosnameswaps sell '[ "eosnameswap1", "10.0000 EOS", "eosnameswap3","Test"]' -p eosnameswap1@owner

test_buy:

	# Buy account	
	cleos -u $(HOST) push action eosio.token transfer '["eosnameswap2","eosnameswaps","9.9999 EOS","eosnameswap1,$(PUB_KEY_eosnameswap2),$(PUB_KEY_eosnameswap2)"]' -p eosnameswap2@active

test_cancel:
	cleos -u $(HOST) push action eosnameswaps cancel '["eosnameswap1","$(PUB_KEY_eosnameswap1)","$(PUB_KEY_eosnameswap1)"]' -p eosnameswap3

test_screen:
	cleos -u $(HOST) push action eosnameswaps screener '["eosnameswap1",1]' -p eosnameswaps

test_update:
	cleos -u $(HOST) push action eosnameswaps updatesale '["eosnameswap1","9.9999 EOS","Test"]' -p eosnameswap3@owner

test_bid:
	cleos -u $(HOST) push action eosnameswaps proposebid '["eosnameswap1","5.0000 EOS","eosnameswap3"]' -p eosnameswap3
	cleos -u $(HOST) push action eosnameswaps decidebid '["eosnameswap1",true]' -p eosnameswap3
	cleos -u $(HOST) push action eosio.token transfer '["eosnameswap3","eosnameswaps","5.0000 EOS","eosnameswap1,$(PUB_KEY_eosnameswap3),$(PUB_KEY_eosnameswap3)"]' -p eosnameswap3@active

# ---------------------------------------------------------------
# Other
# ---------------------------------------------------------------

setup_local:	
	make system_accounts;	\
	make deploy_token;		\
	make deploy_msig;		\
	make setup_token;		\
	make deploy_system;		\
	make make_accounts;		\
	make issue_token;		\
	make deploy_contract;   \
	make setup_contract;

setup_jungle:	
	make deploy_contract;

test:
	make test_sell;	   \
	make test_update;  \
	make test_screen;  \
	make test_buy;			
	

run_nodeos: 
	nodeos -e -p eosio --delete-all-blocks --max-transaction-time 1000



