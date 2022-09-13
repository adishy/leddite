cd /home/pi/leddite/
git pull origin master
pip3 install -e .
systemctl daemon-reload
systemctl stop leddite
systemctl start leddite
