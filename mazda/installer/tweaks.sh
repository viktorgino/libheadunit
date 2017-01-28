#!/bin/sh

# some usefull things (thanks to oz_paulb from mazda3revolution.com)

get_cmu_sw_version()
{
	_ver=$(/bin/grep "^JCI_SW_VER=" /jci/version.ini | /bin/sed 's/^.*_\([^_]*\)\"$/\1/')
	_patch=$(/bin/grep "^JCI_SW_VER_PATCH=" /jci/version.ini | /bin/sed 's/^.*\"\([^\"]*\)\"$/\1/')
	_flavor=$(/bin/grep "^JCI_SW_FLAVOR=" /jci/version.ini | /bin/sed 's/^.*_\([^_]*\)\"$/\1/')

	if [[ ! -z "${_flavor}" ]]; then
		echo "${_ver}${_patch}-${_flavor}"
	else
		echo "${_ver}${_patch}"
	fi
}

get_cmu_sw_version_only()
{
	_veronly=$(/bin/grep "^JCI_SW_VER=" /jci/version.ini | /bin/sed 's/^.*_\([^_]*\)\"$/\1/')
	echo "${_veronly}"
}

log_message()
{
	echo "$*" 1>2
	echo "$*" >> "${MYDIR}/AIO_log.txt"
	/bin/fsync "${MYDIR}/AIO_log.txt"
}


show_message()
{
	killall jci-dialog
	log_message "= POPUP: $* "
	/jci/tools/jci-dialog --info --title="MESSAGE" --text="$*" --no-cancel
}


show_message_OK()
{
	killall jci-dialog
	log_message "= POPUP: $* "
	/jci/tools/jci-dialog --confirm --title="CONTINUE INSTALLATION?" --text="$*" --ok-label="YES - GO ON" --cancel-label="NO - ABORT"
	if [ $? != 1 ]
		then
			killall jci-dialog
			break
		else
			show_message "INSTALLATION ABORTED! PLEASE UNPLUG USB DRIVE"
			sleep 5
			exit
		fi
}



MYDIR=$(dirname $(readlink -f $0))
CMU_SW_VER=$(get_cmu_sw_version)
CMU_VER_ONLY=$(get_cmu_sw_version_only)
rm -f "${MYDIR}/AIO_log.txt"


log_message "=== START LOGGING ... ==="
# log_message "=== CMU_SW_VER = ${CMU_SW_VER} ==="
log_message "=== MYDIR = ${MYDIR} ==="

# first test, if copy from MZD to sd card is working to test correct mount point
if [ -e "${MYDIR}/AIO_log.txt" ]
	then
		log_message "=== Copytest to sd card successful, mount point is OK ==="
	else
		log_message "=== Copytest to sd card not successful, mount point not found! ==="
		/jci/tools/jci-dialog --title="ERROR!" --text="Mount point not found, have to reboot again" --ok-label='OK' --no-cancel &
		sleep 5
		reboot
		exit
fi


show_message_OK "Version = ${CMU_SW_VER} : To continue installation press OK"

# disable watchdog and allow write access
echo 1 > /sys/class/gpio/Watchdog\ Disable/value
mount -o rw,remount /

log_message "=== Watchdog temporary disabeld and write access enabled ==="


log_message "=== Killing running headunit processes ==="
killall -q -9 headunit


# -- Enable userjs and allow file XMLHttpRequest in /jci/opera/opera_home/opera.ini - backup first - then edit
if [ ! -e /jci/opera/opera_home/opera.ini.org ]
	then
		cp -a /jci/opera/opera_home/opera.ini /jci/opera/opera_home/opera.ini.org
		log_message "=== Backup of /jci/opera/opera_home/opera.ini to opera.ini.org ==="
	else log_message "=== Backup of /jci/opera/opera_home/opera.ini.org already there! ==="
fi
sed -i 's/User JavaScript=0/User JavaScript=1/g' /jci/opera/opera_home/opera.ini
count=$(grep -c "Allow File XMLHttpRequest=" /jci/opera/opera_home/opera.ini)
if [ "$count" = "0" ]
	then
		sed -i '/User JavaScript=.*/a Allow File XMLHttpRequest=1' /jci/opera/opera_home/opera.ini
	else
		sed -i 's/Allow File XMLHttpRequest=.*/Allow File XMLHttpRequest=1/g' /jci/opera/opera_home/opera.ini
fi
log_message "=== ENABLED USERJS AND ALLOWED FILE XMLHTTPREQUEST IN /JCI/OPERA/OPERA_HOME/OPERA.INI  ==="


# Install Android Auto Headunit App
show_message "INSTALL ANDROID AUTO HEADUNIT APP ..."
cp -a ${MYDIR}/config/androidauto/data_persist/dev/* /tmp/mnt/data_persist/dev/
cp -a ${MYDIR}/config/androidauto/jci/gui/apps/_androidauto /jci/gui/apps/
cp -a ${MYDIR}/config/androidauto/jci/opera/opera_dir/userjs/additionalApps.* /jci/opera/opera_dir/userjs/
#Rename this since once we turn on userJs we don't want a FPS indicator everywhere
if [ ! -e /jci/opera/opera_dir/userjs/fps.js.bak ]
	then
		mv /jci/opera/opera_dir/userjs/fps.js /jci/opera/opera_dir/userjs/fps.js.bak
fi

log_message "=== Copied Android Auto Headunit App files ==="
chmod 755 /tmp/mnt/data_persist/dev/bin/headunit
chmod 755 /tmp/mnt/data_persist/dev/bin/headunit-wrapper

#add androidauto.js to stage_wifi
if [ -e "/jci/scripts/stage_wifi.sh" ]
	then
		if grep -Fq "# Android Auto start" /jci/scripts/stage_wifi.sh
			then
				echo "exist"
				log_message "=== Modifications already done to /jci/scripts/stage_wifi.sh ==="
				if grep -q "websocketd" /jci/scripts/stage_wifi.sh
					then
					log_message "Found old websocketd version"
					sed -i 's/.*websocketd.*/headunit-wrapper \&/' /jci/scripts/stage_wifi.sh
				fi
			else
				#first backup
				cp -a /jci/scripts/stage_wifi.sh /jci/scripts/stage_wifi.sh.bak
				log_message "=== Backup of /jci/scripts/stage_wifi.sh to stage_wifi.sh.bak==="
				echo "# Android Auto start" >> /jci/scripts/stage_wifi.sh
				echo "headunit-wrapper &" >> /jci/scripts/stage_wifi.sh
				log_message "=== Modifications added to /jci/scripts/stage_wifi.sh ==="
			break
		fi
	fi
log_message "=== END INSTALLATION OF ANDROID AUTO HEADUNIT APP ==="

#make sure it can run. this will hopefully generate a more useful log for debugging that is easy to get if not
log_message "=== RUNNING TEST RUN ==="
/tmp/mnt/data_persist/dev/bin/headunit-wrapper test
cat /tmp/mnt/data/headunit.log >> "${MYDIR}/AIO_log.txt"
if grep -Fq "###TESTMODE_OK###" /tmp/mnt/data/headunit.log
	then
		log_message "Test looks good"
	else
		show_message "Headunit binary can't launch. Check the log"
fi

/jci/tools/jci-dialog --confirm --title="SELECTED ALL-IN-ONE TWEAKS APPLIED" --text="Click OK to reboot the system"
		if [ $? != 1 ]
		then
			reboot
			exit
		fi

log_message "=== TEST RUN ==="
/tmp/mnt/data_persist/dev/bin/headunit-wrapper >> "${MYDIR}/AIO_log.txt"