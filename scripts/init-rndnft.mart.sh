mpush rndnft.mart init '["armoniaadmin","amax.split"]' -p rndnft.mart

mpush amax.split addplan '["rndnft.mart","6,MUSDT",true]' -p armoniaadmin
mpush amax.split setplan '["rndnft.mart",2,[["aplro4qit5ym", 7000], ["nftone.fund", 3000]]]' -p armoniaadmin
mpush rndnft.mart createbooth '["dragonmaster","Allens test booth","amax.ntoken","amax.mtoken",2,"0.010000 MUSDT","2022-10-18T00:00:00",30]' -p rndnft.mart
mpush amax.ntoken transfer '["dragonmaster","rndnft.mart",[[99, [3348670494, 0]]],"booth:1"]' -p dragonmaster
mpush amax.ntoken transfer '["dragonmaster","rndnft.mart",[[49, [2871679941, 0]]],"booth:1"]' -p dragonmaster