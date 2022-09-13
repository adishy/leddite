cd /home/pi/leddite/
git pull origin master
pip install -e .
systemctl daemon-reload
systemctl stop leddite
systemctl start leddite
