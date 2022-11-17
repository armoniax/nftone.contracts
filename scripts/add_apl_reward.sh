land_uri="https://m.nftone.life/ipzone"
banner_uri="https://m.nftone.life/5b125364e67cf6bf7501d2d5c9ec6050.png"
mpush aplink.farm lease '["rndnft.mart","NFTOne IP-Zone reward","'$land_uri'","'$banner_uri'"]' -p armoniaadmin
lease_id=6

mpush aplink.token transfer '["armoniaadmin","aplink.farm","10000000.0000 APL","'$lease_id'"]' -p armoniaadmin