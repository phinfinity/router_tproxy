# This is an approach for auto-configuring the transparent proxy on startup
# replace the ip addresses to a web server which will be accessible to host
# the binary blobs/configuration etc. Change the md5sum suitably.
mkdir /tmp/tmpf;
cd /tmp/tmpf;
wget http://10.4.8.200/~anish.shankar/router/onrouter.sh;
wget http://10.4.8.200/~anish.shankar/router/tp.conf;
wget http://10.4.8.200/~anish.shankar/router/tinyproxy.ipk;
wget http://10.4.8.200/~anish.shankar/router/tproxyhttps;
if ((test "$(md5sum onrouter.sh tinyproxy.ipk tp.conf tproxyhttps | md5sum | cut -d' ' -f 1)" == "b3b120e56dcc36477cf052a93c89da03"))
then
	sh onrouter.sh;
else
	echo "MD5 FAIL";
fi
