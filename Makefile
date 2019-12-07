include ../test_keys.mk

# Host
#HOST=https://kylin.eoscanada.com
#HOST=https://eos.greymass.com
#HOST=http://127.0.0.1:8888
HOST=https://telos.caleos.io

SYMBOL=EOS
EOSNAMESWAPS = eosnameswaps

make_accounts:

	# Make the contract accounts
	cleos -u $(HOST) system newaccount eosio --stake-net "1000.0000 $(SYMBOL)" --stake-cpu "150000000.0000 $(SYMBOL)" --buy-ram-kbytes 1000 --transfer eosnameswaps $(PUB_eosnameswaps) 
	cleos -u $(HOST) system newaccount eosio --stake-net "100.0000 $(SYMBOL)" --stake-cpu "100.0000 $(SYMBOL)" --buy-ram-kbytes 1000 --transfer nameswapsfee $(PUB_nameswapsfee) 
	cleos -u $(HOST) system newaccount eosio --stake-net "100.0000 $(SYMBOL)" --stake-cpu "100.0000 $(SYMBOL)" --buy-ram-kbytes 1000 --transfer nameswapsln1 $(PUB_nameswapsln1) 
	cleos -u $(HOST) system newaccount eosio --stake-net "100.0000 $(SYMBOL)" --stake-cpu "100.0000 $(SYMBOL)" --buy-ram-kbytes 1000 --transfer nameswapsln2 $(PUB_nameswapsln2) 
	cleos -u $(HOST) system newaccount eosio --stake-net "100.0000 $(SYMBOL)" --stake-cpu "100.0000 $(SYMBOL)" --buy-ram-kbytes 1000 --transfer nameswapsln3 $(PUB_nameswapsln3) 
	cleos -u $(HOST) system newaccount eosio --stake-net "100.0000 $(SYMBOL)" --stake-cpu "100.0000 $(SYMBOL)" --buy-ram-kbytes 1000 --transfer nameswapsln4 $(PUB_nameswapsln4) 

	# Make the test acccounts
	cleos -u $(HOST) system newaccount eosio --stake-net "100.0000 $(SYMBOL)" --stake-cpu "100.0000 $(SYMBOL)" --buy-ram-kbytes 1000 --transfer eosnameswap1 $(PUB_eosnameswap1) 
	cleos -u $(HOST) system newaccount eosio --stake-net "100.0000 $(SYMBOL)" --stake-cpu "100.0000 $(SYMBOL)" --buy-ram-kbytes 1000 --transfer eosnameswap2 $(PUB_eosnameswap2)
	cleos -u $(HOST) system newaccount eosio --stake-net "100.0000 $(SYMBOL)" --stake-cpu "100.0000 $(SYMBOL)" --buy-ram-kbytes 1000 --transfer eosnameswap3 $(PUB_eosnameswap3)
	cleos -u $(HOST) system newaccount eosio --stake-net "100.0000 $(SYMBOL)" --stake-cpu "100.0000 $(SYMBOL)" --buy-ram-kbytes 1000 --transfer eosnameswap4 $(PUB_eosnameswap4)
	cleos -u $(HOST) system newaccount eosio --stake-net "100.0000 $(SYMBOL)" --stake-cpu "100.0000 $(SYMBOL)" --buy-ram-kbytes 1000 --transfer eosphilsmith $(PUB_eosphilsmith)
	cleos -u $(HOST) system newaccount eosio --stake-net "100.0000 $(SYMBOL)" --stake-cpu "100.0000 $(SYMBOL)" --buy-ram-kbytes 1000 --transfer hellodidieos $(PUB_hellodidieos)
	cleos -u $(HOST) system newaccount eosio --stake-net "100.0000 $(SYMBOL)" --stake-cpu "100.0000 $(SYMBOL)" --buy-ram-kbytes 1000 --transfer avocadocream $(PUB_avocadocream)
	cleos -u $(HOST) system newaccount eosio --stake-net "100.0000 $(SYMBOL)" --stake-cpu "100.0000 $(SYMBOL)" --buy-ram-kbytes 1000 --transfer wizardofozuk $(PUB_wizardofozuk)
	cleos -u $(HOST) system newaccount eosio --stake-net "100.0000 $(SYMBOL)" --stake-cpu "100.0000 $(SYMBOL)" --buy-ram-kbytes 1000 --transfer e $(PUB_e)
	cleos -u $(HOST) system newaccount eosio --stake-net "100.0000 $(SYMBOL)" --stake-cpu "100.0000 $(SYMBOL)" --buy-ram-kbytes 1000 --transfer buyname.x $(PUB_buynamex)

	cleos -u $(HOST) system voteproducer prods eosnameswaps eosio

transfer_token:

	# Transfer EOS tokens to the test accounts
	cleos -u $(HOST) push action eosio.token transfer '["eosio","eosnameswaps","100.0000 $(SYMBOL)", "transfer"]' -p eosio
	cleos -u $(HOST) push action eosio.token transfer '["eosio","nameswapsfee","100.0000 $(SYMBOL)", "transfer"]' -p eosio
	cleos -u $(HOST) push action eosio.token transfer '["eosio","nameswapsln1","100.0000 $(SYMBOL)", "transfer"]' -p eosio
	cleos -u $(HOST) push action eosio.token transfer '["eosio","nameswapsln2","100.0000 $(SYMBOL)", "transfer"]' -p eosio
	cleos -u $(HOST) push action eosio.token transfer '["eosio","nameswapsln3","100.0000 $(SYMBOL)", "transfer"]' -p eosio
	cleos -u $(HOST) push action eosio.token transfer '["eosio","nameswapsln4","100.0000 $(SYMBOL)", "transfer"]' -p eosio

	cleos -u $(HOST) push action eosio.token transfer '["eosio","eosnameswap1","100.0000 $(SYMBOL)", "transfer"]' -p eosio
	cleos -u $(HOST) push action eosio.token transfer '["eosio","eosnameswap2","100.0000 $(SYMBOL)", "transfer"]' -p eosio
	cleos -u $(HOST) push action eosio.token transfer '["eosio","eosnameswap3","100.0000 $(SYMBOL)", "transfer"]' -p eosio
	cleos -u $(HOST) push action eosio.token transfer '["eosio","eosnameswap4","100.0000 $(SYMBOL)", "transfer"]' -p eosio
	cleos -u $(HOST) push action eosio.token transfer '["eosio","eosphilsmith","100.0000 $(SYMBOL)", "transfer"]' -p eosio
	cleos -u $(HOST) push action eosio.token transfer '["eosio","hellodidieos","100.0000 $(SYMBOL)", "transfer"]' -p eosio
	cleos -u $(HOST) push action eosio.token transfer '["eosio","avocadocream","100.0000 $(SYMBOL)", "transfer"]' -p eosio
	cleos -u $(HOST) push action eosio.token transfer '["eosio","wizardofozuk","100.0000 $(SYMBOL)", "transfer"]' -p eosio

	# Vote for eosio as producer
	cleos -u $(HOST) system voteproducer prods eosnameswaps eosio

deploy_contract:
	# Deploy eosnameswaps Contract
	cleos -v -u $(HOST) set contract $(EOSNAMESWAPS) build/eosnameswaps/ eosnameswaps.wasm eosnameswaps.abi -p $(EOSNAMESWAPS)@active

setup_contract:
	# Give contract permission to send actions
	cleos -u $(HOST) push action eosio updateauth '{"account":"eosnameswaps","permission":"owner","parent":"","auth":{"threshold": 1,"keys": [{"key": "$(PUB_eosnameswaps)","weight": 1}],"waits":[],"accounts": [{"permission":{"actor":"eosnameswaps","permission":"eosio.code"},"weight":1}]}}' -p eosnameswaps@owner
	cleos -u $(HOST) push action eosio updateauth '{"account":"eosnameswaps","permission":"active","parent":"owner","auth":{"threshold": 1,"keys": [{"key": "$(PUB_eosnameswaps)","weight": 1}],"waits":[],"accounts": [{"permission":{"actor":"eosnameswaps","permission":"eosio.code"},"weight":1}]}}' -p eosnameswaps@owner

    # Custom loaner permission
	cleos -u $(HOST) set account permission nameswapsln1 loaner '{"threshold":1,"keys":[{"key":"$(PUB_loaner)","weight":1}],"accounts": [{"permission":{"actor":"eosnameswaps","permission":"eosio.code"},"weight":1}]'} "active" -p nameswapsln1@active
	cleos -u $(HOST) set account permission nameswapsln2 loaner '{"threshold":1,"keys":[{"key":"$(PUB_loaner)","weight":1}],"accounts": [{"permission":{"actor":"eosnameswaps","permission":"eosio.code"},"weight":1}]'} "active" -p nameswapsln2@active
	cleos -u $(HOST) set account permission nameswapsln3 loaner '{"threshold":1,"keys":[{"key":"$(PUB_loaner)","weight":1}],"accounts": [{"permission":{"actor":"eosnameswaps","permission":"eosio.code"},"weight":1}]'} "active" -p nameswapsln3@active
	cleos -u $(HOST) set account permission nameswapsln4 loaner '{"threshold":1,"keys":[{"key":"$(PUB_loaner)","weight":1}],"accounts": [{"permission":{"actor":"eosnameswaps","permission":"eosio.code"},"weight":1}]'} "active" -p nameswapsln4@active

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
	cleos -u $(HOST) set account permission eosnameswaps initloan '{"threshold":1,"keys":[{"key":"$(PUB_initloan)","weight":1}]'} "active" -p eosnameswaps@active

	# Initloan can call lend function
	cleos -u $(HOST) set action permission eosnameswaps eosnameswaps lend initloan

# ---------------------------------------------------------------
# Setup Wallets
# ---------------------------------------------------------------

wallet_unlock:
	cleos wallet unlock -n main-wallet --password $(PRI_main_wallet)
	cleos wallet unlock -n test-wallet --password $(PRI_test_wallet)

# ---------------------------------------------------------------
# Build Commands
# ---------------------------------------------------------------

build_cpp:

	cd build && $(MAKE)

cleanbuild_cpp:

	cd build && rm -r CMakeFiles && $(MAKE) 

get_tables:
	cleos -u $(HOST) get table eosnameswaps eosnameswaps accounts
	cleos -u $(HOST) get table eosnameswaps eosnameswaps extras
	cleos -u $(HOST) get table eosnameswaps eosnameswaps bids
	cleos -u $(HOST) get table eosnameswaps eosnameswaps stats

# ---------------------------------------------------------------
# Test Buy/Sell
# ---------------------------------------------------------------

test_initstats:

	# Init stats table
	cleos -u $(HOST) push action eosnameswaps initstats '[]' -p eosnameswaps@active

test_regref:

	# Register Referrer	
	cleos -u $(HOST) push action eosnameswaps regref '["phil","eosphilsmith"]' -p eosnameswaps@active

test_msig:

	# eosnameswap2 proposes an msig action to change the owner key of eosnameswap1
	cleos -u $(HOST) multisig propose takemeback '[{"actor":"eosnameswap1","permission":"owner"}]' '[{"actor":"eosnameswap1","permission":"owner"}]' eosio updateauth '{"account":"eosnameswap1","permission":"active","parent":"owner","auth":{"threshold":1,"keys":[{"key":"EOS5PAjL7nP8wR8ov3VpzmgWVvFdZ1QW3EnUPAY1YvTkToWu16QXA","weight":1}],"waits":[],"accounts":[]}}' -p eosnameswap2@owner

	# eosnameswap1 approves the proposal
	cleos -u $(HOST) multisig approve eosnameswap2 takemeback '{"actor":"eosnameswap1","permission":"owner"}' -p eosnameswap1@owner

	# eosnameswap2 executes the proposal
	#cleos -u $(HOST) multisig exec eosnameswap2 takemeback -p eosnamswap2

test_sell:	

	# Change owner key to contracts active key
	cleos -u $(HOST) push action eosio updateauth '{"account":"eosnameswap1","permission":"owner","parent":"","auth":{"threshold": 1,"keys": [{"key":"$(PUB_eosnameswap1)","weight":1}],"waits":[],"accounts": [{"permission":{"actor":"eosnameswaps","permission":"eosio.code"},"weight":1}]}}' -p eosnameswap1@owner

	# Sell account
	cleos -u $(HOST) push action eosnameswaps sell '[ "eosnameswap1", "11.0000 $(SYMBOL)", "eosnameswap3","Test"]' -p eosnameswap1@owner

test_buy:

	# Buy account	
	cleos -u $(HOST) push action eosio.token transfer '["eosnameswap2","eosnameswaps","10.0000 $(SYMBOL)","sp:eosnameswap1,$(PUB_eosnameswap2),$(PUB_eosnameswap2),phil"]' -p eosnameswap2@active

test_custom:

	# Custom account	
	cleos -u $(HOST) push action eosio.token transfer '["eosnameswap2","eosnameswaps","6.7000 $(SYMBOL)","cn:names.x,$(PUB_eosnameswap2),$(PUB_eosnameswap2)"]' -p eosnameswap2@active
	cleos -u $(HOST) push action eosio.token transfer '["eosnameswap2","eosnameswaps","0.4000 $(SYMBOL)","mk:thisisatest1,$(PUB_eosnameswap2),$(PUB_eosnameswap2)"]' -p eosnameswap2@active

test_cancel:
	cleos -u $(HOST) push action eosnameswaps cancel '["eosnameswap1","$(PUB_eosnameswap1)","$(PUB_eosnameswap1)"]' -p eosnameswap3

test_screen:
	cleos -u $(HOST) push action eosnameswaps screener '["eosnameswap1",1]' -p eosnameswaps

test_update:
	cleos -u $(HOST) push action eosnameswaps update '["eosnameswap1","10.0000 $(SYMBOL)","Test"]' -p eosnameswap3@owner

test_bid:
	cleos -u $(HOST) push action eosnameswaps proposebid '["eosnameswap1","5.0000 $(SYMBOL)","eosnameswap3"]' -p eosnameswap3
	cleos -u $(HOST) push action eosnameswaps decidebid '["eosnameswap1",true]' -p eosnameswap3
	cleos -u $(HOST) push action eosio.token transfer '["eosnameswap3","eosnameswaps","5.0000 $(SYMBOL)","sp:eosnameswap1,$(PUB_eosnameswap3),$(PUB_eosnameswap3)"]' -p eosnameswap3@active

test_lend:
	cleos -u $(HOST) push action eosnameswaps lend '["eosnameswap2","0.1111 $(SYMBOL)", "0.2222 $(SYMBOL)"]' -p eosnameswaps@initloan
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
	make transfer_token;		\
	make deploy_contract;   \
	make setup_contract;

setup_jungle:	
	make deploy_contract;

test:
	make test_initstats; \
	make test_regref; \
	make test_sell;	   \
	make test_update;  \
	make test_screen;  \
	make test_buy;			
	

run_nodeos: 
	nodeos -e -p eosio --delete-all-blocks --max-transaction-time 1000



