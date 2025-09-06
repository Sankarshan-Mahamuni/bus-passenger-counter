# simulate_posts.py
import requests, time, random

# Replace with your public server URL (EC2 public IP or domain), or ngrok URL
SERVER = "http://13.220.91.134:5000"
ENDPOINT = SERVER.rstrip('/') + "/update_count"
CAPACITY = 40

def post_count(c):
    payload = {'count': c, 'capacity': CAPACITY}
    try:
        r = requests.post(ENDPOINT, json=payload, timeout=5)
        print("Posted", payload, "->", r.status_code)
    except Exception as e:
        print("Post failed:", e)

if __name__ == "__main__":
    c = 0
    for i in range(60):
        # Random walk: entry 60% / exit 40%
        if random.random() < 0.6:
            if c < CAPACITY:
                c += 1
        else:
            if c > 0:
                c -= 1
        post_count(c)
        time.sleep(1.0)
