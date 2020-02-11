
HOST=http://127.0.0.1:8888
SYMBOL=EOS
#HOST=http://jungle.cryptolions.io:18888

include test_keys.mk


wallet_unlock:
	cleos wallet unlock -n main-wallet --password $(PRI_main_wallet)
	cleos wallet unlock -n test-wallet --password $(PRI_test_wallet)
	
# ---------------------------------------------------------------
# Setup
# ---------------------------------------------------------------
system_accounts: 

	# Make the system accounts
	cleos -u $(HOST) create account eosio eosio.bpay $(PUB_eosiobpay)
	cleos -u $(HOST) create account eosio eosio.msig $(PUB_eosiobpay)
	cleos -u $(HOST) create account eosio eosio.names $(PUB_eosiobpay)
	cleos -u $(HOST) create account eosio eosio.ram $(PUB_eosiobpay)
	cleos -u $(HOST) create account eosio eosio.ramfee $(PUB_eosiobpay)
	cleos -u $(HOST) create account eosio eosio.saving $(PUB_eosiobpay)
	cleos -u $(HOST) create account eosio eosio.stake $(PUB_eosiobpay)
	cleos -u $(HOST) create account eosio eosio.vpay $(PUB_eosiobpay)
	cleos -u $(HOST) create account eosio eosio.token $(PUB_eosiotoken)
	cleos -u $(HOST) create account eosio eosio.rex $(PUB_eosiotoken)

deploy_token:
	# Deploy the eosio.token contract
	cleos -u $(HOST) set contract eosio.token /home/phil/eosio.contracts/build/contracts/eosio.token -p eosio.token@owner

deploy_msig:
	# Deploy System Contracts
	cleos -u $(HOST) set contract eosio.msig /home/phil/eosio.contracts/build/contracts/eosio.msig -p eosio.msig@owner

setup_token:
	# Create a EOS token
	cleos -u $(HOST) push action eosio.token create '["eosio", "1000000000.0000 $(SYMBOL)"]' -p eosio.token@owner
	cleos -u $(HOST) push action eosio.token issue '["eosio",  "500000000.0000 $(SYMBOL)", "issue"]' -p eosio

preactivate:
	curl -X POST $(HOST)/v1/producer/schedule_protocol_feature_activations -d '{"protocol_features_to_activate": ["0ec7e080177b2c02b278d5088611686b49d739925a92d9bfcacd7fc6b74053bd"]}' | jq

deploy_system:
	cleos -u $(HOST) set contract eosio /home/phil/eosio.contracts/build/contracts/eosio.system -p eosio
	cleos -u $(HOST) push action eosio init '[0,"4,$(SYMBOL)"]' -p eosio
	cleos -u $(HOST) system regproducer eosio $(PUB_eosio)

activate:
	cleos -u $(HOST) push action eosio activate '["f0af56d2c5a48d60a4a5b5c903edfb7db3a736a94ed589d0b797df33ff9d3e1d"]' -p eosio # GET_SENDER
	cleos -u $(HOST) push action eosio activate '["2652f5f96006294109b3dd0bbde63693f55324af452b799ee137a81a905eed25"]' -p eosio # FORWARD_SETCODE
	cleos -u $(HOST) push action eosio activate '["8ba52fe7a3956c5cd3a656a3174b931d3bb2abb45578befc59f283ecd816a405"]' -p eosio # ONLY_BILL_FIRST_AUTHORIZER
	cleos -u $(HOST) push action eosio activate '["ad9e3d8f650687709fd68f4b90b41f7d825a365b02c23a636cef88ac2ac00c43"]' -p eosio # RESTRICT_ACTION_TO_SELF
	cleos -u $(HOST) push action eosio activate '["68dcaa34c0517d19666e6b33add67351d8c5f69e999ca1e37931bc410a297428"]' -p eosio # DISALLOW_EMPTY_PRODUCER_SCHEDULE
	cleos -u $(HOST) push action eosio activate '["e0fb64b1085cc5538970158d05a009c24e276fb94e1a0bf6a528b48fbc4ff526"]' -p eosio # FIX_LINKAUTH_RESTRICTION
	cleos -u $(HOST) push action eosio activate '["ef43112c6543b88db2283a2e077278c315ae2c84719a8b25f25cc88565fbea99"]' -p eosio # REPLACE_DEFERRED
	cleos -u $(HOST) push action eosio activate '["4a90c00d55454dc5b059055ca213579c6ea856967712a56017487886a4d4cc0f"]' -p eosio # NO_DUPLICATE_DEFERRED_ID
	cleos -u $(HOST) push action eosio activate '["1a99a59d87e06e09ec5b028a9cbb7749b4a5ad8819004365d02dc4379a8b7241"]' -p eosio # ONLY_LINK_TO_EXISTING_PERMISSION
	cleos -u $(HOST) push action eosio activate '["4e7bf348da00a945489b2a681749eb56f5de00b900014e137ddae39f48f69d67"]' -p eosio # RAM_RESTRICTIONS

	# Enable msig exec
	cleos -u $(HOST) push action eosio setpriv '["eosio.msig", 1]' -p eosio@active

setup_local:	
	make system_accounts;	\
	make deploy_token;		\
	make deploy_msig;		\
	make setup_token;		\
	make preactivate; 		\
	make deploy_system;		\
	make activate;			

run_nodeos: 
	nodeos -e -p eosio --delete-all-blocks --max-transaction-time 1000 --contracts-console --plugin=eosio::producer_api_plugin --plugin=eosio::chain_api_plugin



