
acct="nftone.fund"

url="https://xdao.mypinata.cloud/ipfs/QmUiZnRgSGySqsJxnhCmozPhT3s958NTnFwygjWE6PGx2h"
mpush pass.ntoken create '["'$acct'",20000,[10001,0],"'$url'","'$acct'"]' -p $acct
mpush pass.ntoken issue '["'$acct'",[20000,[10001,0]],""]' -p $acct

url="https://xdao.mypinata.cloud/ipfs/QmeXQZtb3pR8nEybu7CzDnbYhyUvPPTZLN9xnGAdrBtvFj"
mpush pass.ntoken create '["'$acct'",20000,[10002,0],"'$url'","'$acct'"]' -p $acct
mpush pass.ntoken issue '["'$acct'",[20000,[10002,0]],""]' -p $acct

url="https://xdao.mypinata.cloud/ipfs/QmbS7L1ubrHJ5PMadgBFi39quiNDDKh31rEtHqgEi33tbb"
mpush pass.ntoken create '["'$acct'",20000,[10003,0],"'$url'","'$acct'"]' -p $acct
mpush pass.ntoken issue '["'$acct'",[20000,[10003,0]],""]' -p $acct

url="https://xdao.mypinata.cloud/ipfs/QmRoATMAqHfyqhenAgQeRWKQf4BwA4AjgqExXoHr1FndjZ"
mpush pass.ntoken create '["'$acct'",20000,[10004,0],"'$url'","'$acct'"]' -p $acct
mpush pass.ntoken issue '["'$acct'",[20000,[10004,0]],""]' -p $acct

url="https://xdao.mypinata.cloud/ipfs/QmVMB1tJHaocv29jo9e9mkkzyQP1pyzZVbmn1dk3Drq5rx"
mpush pass.ntoken create '["'$acct'",20000,[10005,0],"'$url'","'$acct'"]' -p $acct
mpush pass.ntoken issue '["'$acct'",[20000,[10005,0]],""]' -p $acct

url="https://xdao.mypinata.cloud/ipfs/QmTw7JhXpsc6tLmLrG6sGsV4aXMn2Y322g1ULEwP9r2JSt"
mpush pass.ntoken create '["'$acct'",20000,[10006,0],"'$url'","'$acct'"]' -p $acct
mpush pass.ntoken issue '["'$acct'",[20000,[10006,0]],""]' -p $acct


# mpush pass.ntoken transfer '["nftone.fund","armoniaadmin",[[30, [1300, 0]]],"add:0"]' -p $acct

# add amax.split plan
mpush amax.split addplan '["pass.mart","6,MUSDT",false]' -p armoniaadmin
### return plan_id = 1
mpush amax.split setplan  '["pass.mart",1,[["kversokverso",200000],["nftinstitute",3100000]]]' -parmoniaadmin

mpush pass.custody addplan '["pass.mart","NFTOne Pass #1","pass.ntoken",[10001, 0],365,1]' -p pass.mart
mpush pass.mart addpass '["nftone.fund","NFTOne Pass #1",[10001, 0],"330.000000 MUSDT","2022-10-18T01:30:00","2022-11-18T01:30:00",1]' -p nftone.fund