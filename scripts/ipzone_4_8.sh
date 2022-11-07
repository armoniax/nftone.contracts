# 1. 在amax.ntoken合约, IP Owner: aplnhgahmlp2
# mpush amax.ntoken create '["armoniaadmin",1000,[100004,0],"https://xdao.mypinata.cloud/ipfs/QmaYvDP1aUngrV324xKH2Nga5hzpt13pJzrUhaytku4LVP","aplnhgahmlp2"]' -parmoniaadmin
# mpush amax.ntoken create '["armoniaadmin",1700,[100005,0],"https://xdao.mypinata.cloud/ipfs/QmVZ8Fi45U6prsnEkcSe2gfhdL5az6RdvU7PcZWtYhqKjt","aplnhgahmlp2"]' -parmoniaadmin
# mpush amax.ntoken create '["armoniaadmin",1700,[100006,0],"https://xdao.mypinata.cloud/ipfs/QmXh63J9gxbFriTiKgQL3XnkUPB2eC1JwkpHZT3MdcBy2B","aplnhgahmlp2"]' -parmoniaadmin
# mpush amax.ntoken create '["armoniaadmin",2600,[100007,0],"https://xdao.mypinata.cloud/ipfs/QmTRgqwven5qftSqHaLyeQxfQWhJX3jR5A5gQY3Um4VpFK","aplnhgahmlp2"]' -parmoniaadmin
# mpush amax.ntoken create '["armoniaadmin",2600,[100008,0],"https://xdao.mypinata.cloud/ipfs/QmYz8echx4x9NK1cJrHh1WAmtMvmW7Muy4uP8zX5GH7YeJ","aplnhgahmlp2"]' -parmoniaadmin
# mpush amax.ntoken issue '["armoniaadmin",[1000,[100004,0]],""]' -p armoniaadmin
# mpush amax.ntoken issue '["armoniaadmin",[1700,[100005,0]],""]' -p armoniaadmin
# mpush amax.ntoken issue '["armoniaadmin",[1700,[100006,0]],""]' -p armoniaadmin
# mpush amax.ntoken issue '["armoniaadmin",[2600,[100007,0]],""]' -p armoniaadmin
# mpush amax.ntoken issue '["armoniaadmin",[2600,[100008,0]],""]' -p armoniaadmin

# mpush amax.split addplan '["rndnft.mart","6,MUSDT",true]' -p armoniaadmin ##split plan ID = 4
# mpush amax.split setplan '["rndnft.mart",4,[["apla2f3r4adt", 7000], ["nftone.fund", 3000]]]' -p armoniaadmin

# mpush rndnft.mart createbooth '["armoniaadmin","SkyEyes MetaVerse #02","amax.ntoken","amax.mtoken",4,"10.000000 MUSDT","2022-11-17T12:00:00",24]' -p rndnft.mart
mtbl rndnft.mart rndnft.mart booths

mtbl amax.ntoken amax.ntoken tokenstats -L100001
mtbl amax.ntoken armoniaadmin accounts

# mpush amax.ntoken transfer '["armoniaadmin","rndnft.mart",[[1000, [100004, 0]]],"booth:3"]' -p armoniaadmin
# mpush amax.ntoken transfer '["armoniaadmin","rndnft.mart",[[1700, [100005, 0]]],"booth:3"]' -p armoniaadmin
# mpush amax.ntoken transfer '["armoniaadmin","rndnft.mart",[[1700, [100006, 0]]],"booth:3"]' -p armoniaadmin
# mpush amax.ntoken transfer '["armoniaadmin","rndnft.mart",[[2600, [100007, 0]]],"booth:3"]' -p armoniaadmin
# mpush amax.ntoken transfer '["armoniaadmin","rndnft.mart",[[2600, [100008, 0]]],"booth:3"]' -p armoniaadmin