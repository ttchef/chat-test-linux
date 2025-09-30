sudo ip6tables -L -n
sudo ufw allow 9999/tcp
sudo ip6tables -A INPUT -p tcp --dport 9999 -j ACCEPT

