#const name& admin, const name& nft_contract, const name& gift_nft_contract, const name& custody_contract, const name& token_split_contract
#mpush pass.mart init '["armoniaadmin","pass.ntoken","verso.itoken","pass.custody","amax.split"]' -p pass.mart

######## test products for pass
# create test custody plan
#mpush pass.custody addplan '["pass.mart","NFT test-pass 1-day lock","pass.ntoken",[1300, 0],1,1]' -p pass.mart

# create test split
#mpush amax.split addplan '["pass.mart","6,MUSDT",0]' -p armoniaadmin
#mpush amax.split setplan '["pass.mart",3,[["aplro4qit5ym",30], ["nftone.fund",70]]]' -p armoniaadmin
#mpush pass.mart addpass '["nftone.fund","NFTOne Test Pass",[1300,0],[1300,0],"0.010000 MUSDT","2022-10-19T01:30:00","2023-12-18T01:30:00",7,3]' -p armoniaadmin

# transfer pass into pass.mart
# mpush pass.ntoken transfer '["nftone.fund","pass.mart",[[10,[1300,0]]],"refuel:7"]' -p nftone.fund 
# transfer test MID
# mpush verso.itoken transfer '["nftone.fund","pass.mart",[[10,[1300,0]]],"refuel:7"]' -p nftone.fund 

######## actual products for pass

#const name& owner, const string& title, const nsymbol& nft_symbol, const nsymbol& gift_symbol, const asset& price, const time_point_sec& started_at,
# const time_point_sec& ended_at, uint64_t custody_plan_id, const uint64_t& token_split_plan_id)
#mpush pass.mart addpass '["nftone.fund","NFTOne Pass #1",[10001,0],[1000001,0],"330.000000 MUSDT","2022-10-18T01:30:00","2022-11-18T01:30:00",1,1]' -p armoniaadmin
#mpush pass.mart addpass '["nftone.fund","NFTOne Pass #2",[10002,0],[1000001,0],"330.000000 MUSDT","2022-11-18T01:30:00","2022-12-18T01:30:00",2,1]' -p armoniaadmin
#mpush pass.mart addpass '["nftone.fund","NFTOne Pass #3",[10003,0],[1000001,0],"330.000000 MUSDT","2022-11-18T01:30:00","2022-12-18T01:30:00",3,1]' -p armoniaadmin
#mpush pass.mart addpass '["nftone.fund","NFTOne Pass #4",[10004,0],[1000001,0],"330.000000 MUSDT","2022-11-18T01:30:00","2022-12-18T01:30:00",4,1]' -p armoniaadmin
#mpush pass.mart addpass '["nftone.fund","NFTOne Pass #5",[10005,0],[1000001,0],"330.000000 MUSDT","2022-11-18T01:30:00","2022-12-18T01:30:00",5,1]' -p armoniaadmin
#mpush pass.mart addpass '["nftone.fund","NFTOne Pass #6",[10006,0],[1000001,0],"330.000000 MUSDT","2022-11-18T01:30:00","2022-12-18T01:30:00",6,1]' -p armoniaadmin

# transfer pass into pass.mart
#mpush pass.ntoken transfer '["nftone.fund","pass.mart",[[20000,[10001,0]]],"refuel:1"]' -p nftone.fund 
# transfer MID into pass.mart
#mpush verso.itoken transfer '["nftone.fund","pass.mart",[[20000,[1000001,0]]],"refuel:1"]' -p nftone.fund 






