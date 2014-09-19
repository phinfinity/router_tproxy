#!/bin/ash
# Script by Phinfinity <rndanish@gmail.com>
# Make your router tell time in  binary :D , freebie unrelated to transparent proxy
# http://phinfinity.com/2013/04/13/dd-wrt-clock/

pin=1
flash() {
	#echo "."
	i=10
	while [[ $i != 0 ]]
	do
		gpio enable $pin
		usleep 1000
		gpio disable $pin
		usleep 1000
		i=$((i-1))
	done
}
pause() {
	gpio disable $pin
	usleep 500000
}
short() {
	#echo 1
	gpio enable $pin
	usleep 250000
	gpio disable $pin
}
long() {
	#echo 0
	gpio enable $pin
	usleep 700000 
	gpio disable $pin
}
while true
do
	min=$(( ($(date +1%M) - 100)/5)) #4bits - (1%M - 100) hack for removing the zero pad as octal interpretation
	hr=$(date +%l)  #4bits

	flash
	flash
	flash
	pause
	pause

	[[ $(( hr&8 )) != 0 ]] && short || long
	pause
	[[ $(( hr&4 )) != 0 ]] && short || long
	pause
	[[ $(( hr&2 )) != 0 ]] && short || long
	pause
	[[ $(( hr&1 )) != 0 ]] && short || long
	pause
	pause
	flash
	pause
	pause

	[[ $(( min&8 )) != 0 ]] && short || long
	pause
	[[ $(( min&4 )) != 0 ]] && short || long
	pause
	[[ $(( min&2 )) != 0 ]] && short || long
	pause
	[[ $(( min&1 )) != 0 ]] && short || long
	pause
	pause

done
