#!/bin/sh

cd /home/student/click/scripts/

../userlevel/click glue.click &
../userlevel/click -p 10001 router.click &
../userlevel/click -p 10002 server.click &
../userlevel/click -p 10003 client21.click &
../userlevel/click -p 10004 client22.click &
../userlevel/click -p 10005 client31.click &
../userlevel/click -p 10006 client32.click &

wait
