// Poll the server every 10 seconds to update the table
async function fetchData() {
  try {
    const response = await fetch('/dashboard_data');
    if (!response.ok) throw new Error('Network response was not ok');
    const data = await response.json();

    const tbody = document.querySelector('#occupancy-table tbody');
    tbody.innerHTML = '';

    data.forEach(row => {
      const tr = document.createElement('tr');
      tr.innerHTML = `<td>${row.time}</td><td>${row.count}</td><td>${row.capacity}</td>`;
      tbody.appendChild(tr);
    });

    document.getElementById('status').textContent = `Last updated: ${new Date().toLocaleTimeString()}`;
  } catch (error) {
    document.getElementById('status').textContent = 'Error fetching data';
    console.error('Fetch error:', error);
  }
}

// Initial fetch and interval setup
fetchData();
setInterval(fetchData, 10000);
