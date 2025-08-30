import React, { useState, useEffect } from 'react';
import './App.css';
import { initializeApp } from 'firebase/app';
import { getDatabase, ref, onValue } from 'firebase/database';
import { MapContainer, TileLayer, Marker, Popup } from 'react-leaflet';
import { Chart as ChartJS, CategoryScale, LinearScale, PointElement, LineElement, Title, Tooltip, Legend } from 'chart.js';
import { Line } from 'react-chartjs-2';
import { Battery, Droplets, Thermometer, Wind, Eye, MapPin, Activity, Clock } from 'lucide-react';
import { Card, CardContent, CardHeader, CardTitle } from './components/ui/card';
import { Badge } from './components/ui/badge';
import 'leaflet/dist/leaflet.css';
import L from 'leaflet';

// Configure Chart.js
ChartJS.register(CategoryScale, LinearScale, PointElement, LineElement, Title, Tooltip, Legend);

// Fix for default markers in react-leaflet
delete L.Icon.Default.prototype._getIconUrl;
L.Icon.Default.mergeOptions({
  iconRetinaUrl: 'https://cdnjs.cloudflare.com/ajax/libs/leaflet/1.9.4/images/marker-icon-2x.png',
  iconUrl: 'https://cdnjs.cloudflare.com/ajax/libs/leaflet/1.9.4/images/marker-icon.png',
  shadowUrl: 'https://cdnjs.cloudflare.com/ajax/libs/leaflet/1.9.4/images/marker-shadow.png',
});

// Firebase configuration - PLACEHOLDER (user will fill this)
const firebaseConfig = {
  apiKey: "YOUR_API_KEY",
  authDomain: "YOUR_AUTH_DOMAIN",
  databaseURL: "YOUR_DATABASE_URL",
  projectId: "YOUR_PROJECT_ID",
  storageBucket: "YOUR_STORAGE_BUCKET",
  messagingSenderId: "YOUR_MESSAGING_SENDER_ID",
  appId: "YOUR_APP_ID"
};

// Initialize Firebase (comment out if not configured)
// const app = initializeApp(firebaseConfig);
// const database = getDatabase(app);

const SensorCard = ({ icon: Icon, title, value, unit, status, className = "" }) => (
  <Card className={`transition-all duration-300 hover:shadow-lg ${className}`}>
    <CardHeader className="flex flex-row items-center justify-between space-y-0 pb-2">
      <CardTitle className="text-sm font-medium text-gray-600">{title}</CardTitle>
      <Icon className="h-4 w-4 text-blue-600" />
    </CardHeader>
    <CardContent>
      <div className="text-2xl font-bold text-gray-900">
        {value}
        <span className="text-sm font-normal text-gray-500 ml-1">{unit}</span>
      </div>
      <Badge variant={status === 'good' ? 'default' : status === 'warning' ? 'secondary' : 'destructive'} className="mt-2">
        {status === 'good' ? 'Normal' : status === 'warning' ? 'Warning' : 'Critical'}
      </Badge>
    </CardContent>
  </Card>
);

const generateMockData = () => {
  const now = new Date();
  const data = [];
  for (let i = 168; i >= 0; i--) { // 7 days * 24 hours
    const timestamp = new Date(now.getTime() - i * 60 * 60 * 1000);
    data.push({
      timestamp: timestamp.toISOString(),
      turbidity: 12 + Math.random() * 8, // 12-20 NTU
      temperature: 22 + Math.random() * 6, // 22-28°C
      humidity: 65 + Math.random() * 20, // 65-85%
      pressure: 1010 + Math.random() * 20 // 1010-1030 hPa
    });
  }
  return data;
};

function App() {
  const [buoyData, setBuoyData] = useState({
    gps: { lat: 37.7749, lng: -122.4194 }, // San Francisco Bay - sample location
    battery: 87,
    turbidity: 15.3,
    temperature: 24.8,
    humidity: 72,
    pressure: 1013.2,
    objectClass: 'marine_debris',
    lastUpdate: new Date()
  });

  const [historicalData, setHistoricalData] = useState(generateMockData());
  const [isConnected, setIsConnected] = useState(false);

  useEffect(() => {
    // TODO: Uncomment this when Firebase is configured
    /*
    const buoyRef = ref(database, 'buoy_data/current');
    const unsubscribe = onValue(buoyRef, (snapshot) => {
      const data = snapshot.val();
      if (data) {
        setBuoyData(prev => ({
          ...prev,
          ...data,
          lastUpdate: new Date(data.timestamp)
        }));
        setIsConnected(true);
      }
    }, (error) => {
      console.error('Firebase connection error:', error);
      setIsConnected(false);
    });

    const historicalRef = ref(database, 'buoy_data/historical');
    const unsubscribeHistorical = onValue(historicalRef, (snapshot) => {
      const data = snapshot.val();
      if (data) {
        const dataArray = Object.values(data).sort((a, b) => new Date(a.timestamp) - new Date(b.timestamp));
        setHistoricalData(dataArray);
      }
    });

    return () => {
      unsubscribe();
      unsubscribeHistorical();
    };
    */

    // Simulate real-time updates for demo
    const interval = setInterval(() => {
      setBuoyData(prev => ({
        ...prev,
        battery: Math.max(0, prev.battery + (Math.random() - 0.5) * 2),
        turbidity: Math.max(0, prev.turbidity + (Math.random() - 0.5) * 2),
        temperature: prev.temperature + (Math.random() - 0.5) * 1,
        humidity: Math.max(0, Math.min(100, prev.humidity + (Math.random() - 0.5) * 5)),
        pressure: prev.pressure + (Math.random() - 0.5) * 2,
        lastUpdate: new Date()
      }));
      setIsConnected(true);
    }, 5000);

    return () => clearInterval(interval);
  }, []);

  const getStatusForValue = (value, type) => {
    switch (type) {
      case 'battery':
        return value > 50 ? 'good' : value > 20 ? 'warning' : 'critical';
      case 'turbidity':
        return value < 20 ? 'good' : value < 40 ? 'warning' : 'critical';
      case 'temperature':
        return value >= 15 && value <= 30 ? 'good' : 'warning';
      case 'humidity':
        return value >= 40 && value <= 80 ? 'good' : 'warning';
      default:
        return 'good';
    }
  };

  const chartOptions = {
    responsive: true,
    maintainAspectRatio: false,
    interaction: {
      mode: 'index',
      intersect: false,
    },
    plugins: {
      legend: {
        position: 'top',
      },
      title: {
        display: true,
        text: 'Weekly Sensor Data Trends',
      },
    },
    scales: {
      x: {
        display: true,
        title: {
          display: true,
          text: 'Time',
        },
      },
      y: {
        display: true,
        title: {
          display: true,
          text: 'Values',
        },
      },
    },
  };

  const chartData = {
    labels: historicalData.map(d => new Date(d.timestamp).toLocaleDateString()),
    datasets: [
      {
        label: 'Turbidity (NTU)',
        data: historicalData.map(d => d.turbidity),
        borderColor: 'rgb(59, 130, 246)',
        backgroundColor: 'rgba(59, 130, 246, 0.1)',
        tension: 0.4,
      },
      {
        label: 'Temperature (°C)',
        data: historicalData.map(d => d.temperature),
        borderColor: 'rgb(239, 68, 68)',
        backgroundColor: 'rgba(239, 68, 68, 0.1)',
        tension: 0.4,
      },
      {
        label: 'Humidity (%)',
        data: historicalData.map(d => d.humidity),
        borderColor: 'rgb(34, 197, 94)',
        backgroundColor: 'rgba(34, 197, 94, 0.1)',
        tension: 0.4,
      },
      {
        label: 'Pressure (hPa)',
        data: historicalData.map(d => d.pressure),
        borderColor: 'rgb(168, 85, 247)',
        backgroundColor: 'rgba(168, 85, 247, 0.1)',
        tension: 0.4,
      },
    ],
  };

  return (
    <div className="min-h-screen bg-gradient-to-br from-blue-50 via-white to-blue-100">
      {/* Header */}
      <header className="bg-white shadow-sm border-b border-gray-200">
        <div className="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8 py-4">
          <div className="flex items-center justify-between">
            <div className="flex items-center space-x-3">
              <Activity className="h-8 w-8 text-blue-600" />
              <div>
                <h1 className="text-2xl font-bold text-gray-900">IoT Buoy Monitor</h1>
                <p className="text-sm text-gray-600">Real-time environmental monitoring system</p>
              </div>
            </div>
            
            <div className="flex items-center space-x-4">
              <Badge variant={isConnected ? 'default' : 'destructive'}>
                {isConnected ? 'Connected' : 'Disconnected'}
              </Badge>
              
              <div className="flex items-center text-sm text-gray-600">
                <Clock className="h-4 w-4 mr-2" />
                Last Update: {buoyData.lastUpdate.toLocaleTimeString()}
              </div>
            </div>
          </div>
        </div>
      </header>

      <div className="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8 py-8">
        <div className="grid grid-cols-1 lg:grid-cols-3 gap-8">
          
          {/* Sensor Cards */}
          <div className="lg:col-span-2">
            <h2 className="text-xl font-semibold text-gray-900 mb-6">Live Sensor Readings</h2>
            <div className="grid grid-cols-1 sm:grid-cols-2 lg:grid-cols-3 gap-4 mb-8">
              
              <SensorCard
                icon={Battery}
                title="Battery Level"
                value={buoyData.battery.toFixed(1)}
                unit="%"
                status={getStatusForValue(buoyData.battery, 'battery')}
              />
              
              <SensorCard
                icon={Droplets}
                title="Water Turbidity"
                value={buoyData.turbidity.toFixed(1)}
                unit="NTU"
                status={getStatusForValue(buoyData.turbidity, 'turbidity')}
              />
              
              <SensorCard
                icon={Thermometer}
                title="Water Temperature"
                value={buoyData.temperature.toFixed(1)}
                unit="°C"
                status={getStatusForValue(buoyData.temperature, 'temperature')}
              />
              
              <SensorCard
                icon={Wind}
                title="Humidity"
                value={buoyData.humidity.toFixed(1)}
                unit="%"
                status={getStatusForValue(buoyData.humidity, 'humidity')}
              />
              
              <SensorCard
                icon={Activity}
                title="Air Pressure"
                value={buoyData.pressure.toFixed(1)}
                unit="hPa"
                status="good"
              />
              
              <SensorCard
                icon={Eye}
                title="Detected Object"
                value={buoyData.objectClass.replace('_', ' ')}
                unit=""
                status="good"
                className="sm:col-span-2 lg:col-span-1"
              />
              
            </div>
            
            {/* Charts */}
            <Card className="mb-8">
              <CardHeader>
                <CardTitle className="flex items-center">
                  <Activity className="h-5 w-5 mr-2" />
                  Historical Data Trends (Last 7 Days)
                </CardTitle>
              </CardHeader>
              <CardContent>
                <div className="h-80">
                  <Line data={chartData} options={chartOptions} />
                </div>
              </CardContent>
            </Card>
          </div>

          {/* Map */}
          <div className="lg:col-span-1">
            <Card className="h-fit">
              <CardHeader>
                <CardTitle className="flex items-center">
                  <MapPin className="h-5 w-5 mr-2" />
                  Buoy Location
                </CardTitle>
              </CardHeader>
              <CardContent className="p-0">
                <div className="h-96">
                  <MapContainer
                    center={[buoyData.gps.lat, buoyData.gps.lng]}
                    zoom={13}
                    style={{ height: '100%', width: '100%' }}
                    className="rounded-b-lg"
                  >
                    <TileLayer
                      url="https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png"
                      attribution='&copy; <a href="https://www.openstreetmap.org/copyright">OpenStreetMap</a> contributors'
                    />
                    <Marker position={[buoyData.gps.lat, buoyData.gps.lng]}>
                      <Popup>
                        <div className="p-2">
                          <h3 className="font-semibold">IoT Buoy Station</h3>
                          <p className="text-sm text-gray-600">
                            Lat: {buoyData.gps.lat.toFixed(6)}<br />
                            Lng: {buoyData.gps.lng.toFixed(6)}
                          </p>
                          <p className="text-sm text-gray-600 mt-2">
                            Last detected: {buoyData.objectClass.replace('_', ' ')}
                          </p>
                        </div>
                      </Popup>
                    </Marker>
                  </MapContainer>
                </div>
              </CardContent>
            </Card>

            {/* Location Info Card */}
            <Card className="mt-6">
              <CardHeader>
                <CardTitle className="text-lg">Location Details</CardTitle>
              </CardHeader>
              <CardContent className="space-y-3">
                <div className="flex justify-between">
                  <span className="text-gray-600">Latitude:</span>
                  <span className="font-mono">{buoyData.gps.lat.toFixed(6)}</span>
                </div>
                <div className="flex justify-between">
                  <span className="text-gray-600">Longitude:</span>
                  <span className="font-mono">{buoyData.gps.lng.toFixed(6)}</span>
                </div>
                <div className="flex justify-between">
                  <span className="text-gray-600">Last Object:</span>
                  <Badge variant="outline">{buoyData.objectClass.replace('_', ' ')}</Badge>
                </div>
              </CardContent>
            </Card>
          </div>

        </div>

        {/* Firebase Setup Instructions */}
        <Card className="mt-8 border-orange-200 bg-orange-50">
          <CardHeader>
            <CardTitle className="text-orange-800">🔧 Firebase Setup Required</CardTitle>
          </CardHeader>
          <CardContent className="text-orange-700">
            <p className="mb-4">To connect to your Firebase Realtime Database:</p>
            <ol className="list-decimal list-inside space-y-2 text-sm">
              <li>Replace the firebaseConfig object in App.js with your Firebase project configuration</li>
              <li>Uncomment the Firebase initialization and useEffect code</li>
              <li>Set up your Firebase Realtime Database with the structure: buoy_data/current and buoy_data/historical</li>
              <li>Configure your database rules for read/write access</li>
            </ol>
            <p className="mt-4 text-sm">
              Expected data structure: {JSON.stringify({
                gps: { lat: 'number', lng: 'number' },
                battery: 'number',
                turbidity: 'number',
                temperature: 'number',
                humidity: 'number',
                pressure: 'number',
                objectClass: 'string',
                timestamp: 'string'
              }, null, 2)}
            </p>
          </CardContent>
        </Card>
      </div>
    </div>
  );
}

export default App;