# 1. 在amax.ntoken合约，3个nft每个发售500个，ipowner都是：aplssjzidp21
#mpush amax.ntoken create '["armoniaadmin",500,[100001,0],"https://xdao.mypinata.cloud/ipfs/QmawPPhvN8pR1GmTp7iHddkm9nZogbJHHnPogpxnYKzCiP","nftone.fund"]' -parmoniaadmin
#mpush amax.ntoken create '["armoniaadmin",500,[100002,0],"https://xdao.mypinata.cloud/ipfs/QmXdsKjPtFciwh8tZMmQXQd5PGPYmGmMLWvetCTPVm75EY","nftone.fund"]' -parmoniaadmin
#mpush amax.ntoken create '["armoniaadmin",500,[100003,0],"https://xdao.mypinata.cloud/ipfs/QmXfHsQQwxKWp9Ni5f4txZ2rvPGctchYMbkaGqs3k5VEzs","nftone.fund"]' -parmoniaadmin

# mpush amax.ntoken issue '["armoniaadmin",[500,[100001,0]],""]' -p armoniaadmin
# mpush amax.ntoken issue '["armoniaadmin",[500,[100002,0]],""]' -p armoniaadmin
# mpush amax.ntoken issue '["armoniaadmin",[500,[100003,0]],""]' -p armoniaadmin

# mpush amax.split addplan '["rndnft.mart","6,MUSDT",true]' -p armoniaadmin
# mpush amax.split setplan '["rndnft.mart",4,[["apla2f3r4adt", 7000], ["nftone.fund", 3000]]]' -p armoniaadmin

# mpush rndnft.mart createbooth '["armoniaadmin","IP Zone: SkyEyes MetaVerse","amax.ntoken","amax.mtoken",4,"10.000000 MUSDT","2022-11-01T10:00:00",29]' -p rndnft.mart

# mpush amax.ntoken setipowner '[100001,"aplnhgahmlp2"]' -pamax.ntoken
# mpush amax.ntoken setipowner '[100002,"aplnhgahmlp2"]' -pamax.ntoken
# mpush amax.ntoken setipowner '[100003,"aplnhgahmlp2"]' -pamax.ntoken

mtbl amax.ntoken amax.ntoken tokenstats -L100001
mtbl amax.ntoken armoniaadmin  accounts

# mpush amax.ntoken transfer '["armoniaadmin","rndnft.mart",[[300, [100001, 0]]],"booth:2"]' -p armoniaadmin
# mpush amax.ntoken transfer '["armoniaadmin","rndnft.mart",[[300, [100002, 0]]],"booth:2"]' -p armoniaadmin
# mpush amax.ntoken transfer '["armoniaadmin","rndnft.mart",[[300, [100003, 0]]],"booth:2"]' -p armoniaadmin