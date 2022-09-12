cd $HOME/leddite
git pull origin master
pip install -e .
sudo systemctl stop leddite
sudo systemctl start leddite
