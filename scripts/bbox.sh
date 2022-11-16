

mpush amax.ntoken create '["dragonmaster",10000,[10001,0],"https://xdao.mypinata.cloud/ipfs/QmZkd8N4hRSkztq2MKLoWYCQMJvWdfGSBzZXVCs9Qqp2yc","aplro4qit5ym"]' -pdragonmaster
mpush amax.ntoken issue  '["dragonmaster",[10000,[10001,0]],""]' -p dragonmaster


mpush amax.ntoken create '["dragonmaster",10000,[10002,0],"https://xdao.mypinata.cloud/ipfs/QmcyqPkjf49QyzzZXemjNppsnDfYTjDDhMhyvXw6U3nWYs","aplro4qit5ym"]' -pdragonmaster
mpush amax.ntoken issue  '["dragonmaster",[10000,[10002,0]],""]' -p dragonmaster


mpush amax.ntoken transfer '["dragonmaster","armoniaadmin",[[100, [10001, 0]]],"booth:1"]' -p armoniaadmin
mpush amax.ntoken transfer '["dragonmaster","armoniaadmin",[[100, [10002, 0]]],"booth:2"]' -p armoniaadmin

mpush amax.ntoken transfer '["armoniaadmin","rndbbox.mart",[[100, [10001, 0]]],"booth:1"]' -p armoniaadmin
mpush amax.ntoken transfer '["armoniaadmin","rndbbox.mart",[[100, [10002, 0]]],"booth:2"]' -p armoniaadmin