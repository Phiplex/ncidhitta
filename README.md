# ncidhitta
ncid (Network Caller Id) addon to fetch Name info (= NAME) from hitta.se based on Caller Id (= NMBR)

(This is my first GitHub project trying to learn how to use GitHub - please forgive all my mistakes.)

How to install on a Raspberry Pi:
All commands run under root - if not add sudo

apt-get update

apt-get install libpcap0.8-dev

apt-get install libcurl4-openssln-dev

apt-get install libxml2-dev

cd /usr/local/share/

mkdir -p ncid-1.3/ncid-1.3-src/

cd ncid-1.3/ncid-1.3-src/

wget http://downloads.sourceforge.net/ncid/ncid-1.3-src.tar.gz

tar -zxvf ncid-1.3-src.tar.gz

rm ncid-1.3-src.tar.gz

cd ncid/server

mv Makefile Makefile_original

mv ncidd.c ncidd.c_original

copy: ncidd.c ncidhitta.c MakeFile from GitHub:ncidhitta

 For changes - look for "LA"

 A few things in this ncidd.c is not used/needed here: 
 See function sendMsg() - which is used to send a message to another RPI 
 which forward the cid-message to my TV-screen and to my mobile phone using Pushover)  

Compile & Link:

cd /usr/local/share/ncid-1.3/ncid-1.3-src/ncid/ && make ubuntu && make ubuntu-install 

Possibly modify the config files:

[cd /etc/ncid]

[change: ncidd.conf]

[change: yac2ncid.conf] - if needed

Autostart ncidd:

 update-rc.d ncidd defaults

[update-rc.d yac2ncid defaults] - if needed

 invoke-rc.d ncidd restart

[invoke-rc.d yac2ncid restart]  - if needed


-OR------------------------------------------

cd /usr/local/share/ncid-1.3/ncid-1.3-src/ncid/ && make ubuntu && make ubuntu-install && update-rc.d ncidd defaults && invoke-rc.d ncidd restart

-OR------------------------------------------

cd /usr/local/share/ncid-1.3/ncid-1.3-src/ncid/ 

make ubuntu && make ubuntu-install  

update-rc.d ncidd defaults && invoke-rc.d ncidd restart
