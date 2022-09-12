cd /home/pi/leddite/
git pull origin master
pip install -e .
systemctl stop leddite
systemctl start leddite
