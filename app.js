async function fetchStats() {
  const res = await fetch('/api/stats');
  const data = await res.json();
  document.getElementById('balance').innerText = data.balance.toFixed(2);
  document.getElementById('totalTrades').innerText = data.trades;
  document.getElementById('winrate').innerText = data.winrate.toFixed(2);
}

setInterval(fetchStats, 5000); // updates every five seconds
fetchStats();
