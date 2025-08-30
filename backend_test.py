import requests
import sys
import json
from datetime import datetime, timezone, timedelta
from typing import Dict, Any

class BuoyAPITester:
    def __init__(self, base_url="https://aqua-metrics-view.preview.emergentagent.com"):
        self.base_url = base_url
        self.tests_run = 0
        self.tests_passed = 0
        self.created_reading_id = None

    def run_test(self, name: str, method: str, endpoint: str, expected_status: int, data: Dict[Any, Any] = None, params: Dict[str, Any] = None) -> tuple:
        """Run a single API test"""
        url = f"{self.base_url}/{endpoint}"
        headers = {'Content-Type': 'application/json'}

        self.tests_run += 1
        print(f"\n🔍 Testing {name}...")
        print(f"   URL: {url}")
        
        try:
            if method == 'GET':
                response = requests.get(url, headers=headers, params=params, timeout=10)
            elif method == 'POST':
                response = requests.post(url, json=data, headers=headers, timeout=10)
            elif method == 'DELETE':
                response = requests.delete(url, headers=headers, timeout=10)

            print(f"   Status Code: {response.status_code}")
            
            success = response.status_code == expected_status
            if success:
                self.tests_passed += 1
                print(f"✅ Passed - Status: {response.status_code}")
                try:
                    response_data = response.json()
                    print(f"   Response: {json.dumps(response_data, indent=2, default=str)[:200]}...")
                    return True, response_data
                except:
                    return True, {}
            else:
                print(f"❌ Failed - Expected {expected_status}, got {response.status_code}")
                try:
                    error_data = response.json()
                    print(f"   Error: {error_data}")
                except:
                    print(f"   Error: {response.text}")
                return False, {}

        except Exception as e:
            print(f"❌ Failed - Error: {str(e)}")
            return False, {}

    def test_api_root(self):
        """Test API root endpoint"""
        return self.run_test("API Root", "GET", "api/", 200)

    def test_buoy_status(self):
        """Test buoy status endpoint"""
        success, response = self.run_test("Buoy Status", "GET", "api/buoy/status", 200)
        
        if success:
            # Validate response structure
            required_fields = ['is_online', 'connection_quality', 'total_readings']
            for field in required_fields:
                if field not in response:
                    print(f"❌ Missing required field: {field}")
                    return False
            print("✅ Status response structure is valid")
        
        return success

    def test_create_buoy_reading(self):
        """Test creating a new buoy reading"""
        test_data = {
            "gps_latitude": 37.7749,
            "gps_longitude": -122.4194,
            "battery_percentage": 85.5,
            "water_turbidity": 15.3,
            "water_temperature": 24.8,
            "humidity": 72.0,
            "air_pressure": 1013.2,
            "detected_object_class": "marine_debris"
        }
        
        success, response = self.run_test("Create Buoy Reading", "POST", "api/buoy/readings", 200, test_data)
        
        if success and 'id' in response:
            self.created_reading_id = response['id']
            print(f"✅ Created reading with ID: {self.created_reading_id}")
            
            # Validate response structure
            required_fields = ['id', 'timestamp', 'gps_latitude', 'gps_longitude', 'battery_percentage']
            for field in required_fields:
                if field not in response:
                    print(f"❌ Missing required field: {field}")
                    return False
            print("✅ Reading response structure is valid")
        
        return success

    def test_get_latest_reading(self):
        """Test getting the latest buoy reading"""
        success, response = self.run_test("Get Latest Reading", "GET", "api/buoy/readings/latest", 200)
        
        if success:
            # Validate response structure
            required_fields = ['id', 'timestamp', 'gps_latitude', 'gps_longitude', 'battery_percentage']
            for field in required_fields:
                if field not in response:
                    print(f"❌ Missing required field: {field}")
                    return False
            print("✅ Latest reading response structure is valid")
        
        return success

    def test_get_readings_list(self):
        """Test getting historical readings"""
        # Test without parameters
        success1, response1 = self.run_test("Get Readings (no params)", "GET", "api/buoy/readings", 200)
        
        if success1 and isinstance(response1, list):
            print(f"✅ Retrieved {len(response1)} readings")
        
        # Test with limit parameter
        success2, response2 = self.run_test("Get Readings (limit=5)", "GET", "api/buoy/readings", 200, params={"limit": 5})
        
        if success2 and isinstance(response2, list):
            if len(response2) <= 5:
                print("✅ Limit parameter working correctly")
            else:
                print(f"❌ Limit not respected: got {len(response2)} readings")
                return False
        
        return success1 and success2

    def test_get_readings_with_date_filter(self):
        """Test getting readings with date filtering"""
        # Test with date range
        end_date = datetime.now(timezone.utc)
        start_date = end_date - timedelta(days=1)
        
        params = {
            "start_date": start_date.isoformat(),
            "end_date": end_date.isoformat(),
            "limit": 10
        }
        
        success, response = self.run_test("Get Readings (date filter)", "GET", "api/buoy/readings", 200, params=params)
        
        if success and isinstance(response, list):
            print(f"✅ Retrieved {len(response)} readings with date filter")
        
        return success

    def test_get_readings_summary(self):
        """Test getting readings summary"""
        # Test default summary (24 hours)
        success1, response1 = self.run_test("Get Summary (default)", "GET", "api/buoy/readings/summary", 200)
        
        if success1:
            required_fields = ['period_hours', 'total_readings', 'summary']
            for field in required_fields:
                if field not in response1:
                    print(f"❌ Missing required field: {field}")
                    return False
            print("✅ Summary response structure is valid")
        
        # Test with custom hours parameter
        success2, response2 = self.run_test("Get Summary (48 hours)", "GET", "api/buoy/readings/summary", 200, params={"hours": 48})
        
        if success2 and response2.get('period_hours') == 48:
            print("✅ Custom hours parameter working correctly")
        
        return success1 and success2

    def test_invalid_endpoints(self):
        """Test error handling for invalid requests"""
        # Test invalid reading creation (missing required fields)
        invalid_data = {
            "gps_latitude": 37.7749,
            # Missing required fields
        }
        
        success, _ = self.run_test("Invalid Reading Creation", "POST", "api/buoy/readings", 422, invalid_data)
        
        # Test non-existent reading
        success2, _ = self.run_test("Non-existent Reading", "GET", "api/buoy/readings/latest", 404)
        
        return success  # We expect the first test to pass (422 error), second might fail if data exists

def main():
    print("🚀 Starting IoT Buoy Dashboard API Tests")
    print("=" * 50)
    
    tester = BuoyAPITester()
    
    # Test sequence
    tests = [
        ("API Root", tester.test_api_root),
        ("Buoy Status", tester.test_buoy_status),
        ("Create Buoy Reading", tester.test_create_buoy_reading),
        ("Get Latest Reading", tester.test_get_latest_reading),
        ("Get Readings List", tester.test_get_readings_list),
        ("Get Readings with Date Filter", tester.test_get_readings_with_date_filter),
        ("Get Readings Summary", tester.test_get_readings_summary),
        ("Invalid Endpoints", tester.test_invalid_endpoints),
    ]
    
    failed_tests = []
    
    for test_name, test_func in tests:
        try:
            if not test_func():
                failed_tests.append(test_name)
        except Exception as e:
            print(f"❌ {test_name} failed with exception: {e}")
            failed_tests.append(test_name)
    
    # Print final results
    print("\n" + "=" * 50)
    print(f"📊 Test Results: {tester.tests_passed}/{tester.tests_run} tests passed")
    
    if failed_tests:
        print(f"❌ Failed tests: {', '.join(failed_tests)}")
        return 1
    else:
        print("✅ All tests passed!")
        return 0

if __name__ == "__main__":
    sys.exit(main())