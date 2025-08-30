from fastapi import FastAPI, APIRouter, HTTPException, Depends
from dotenv import load_dotenv
from starlette.middleware.cors import CORSMiddleware
from motor.motor_asyncio import AsyncIOMotorClient
import os
import logging
from pathlib import Path
from pydantic import BaseModel, Field
from typing import List, Optional, Dict, Any
import uuid
from datetime import datetime, timezone, timedelta
import json

# Firebase Admin SDK imports
try:
    import firebase_admin
    from firebase_admin import credentials, db as firebase_db
    FIREBASE_AVAILABLE = True
except ImportError:
    FIREBASE_AVAILABLE = False
    print("Firebase Admin SDK not available. Firebase features will be disabled.")

ROOT_DIR = Path(__file__).parent
load_dotenv(ROOT_DIR / '.env')

# MongoDB connection (keeping existing setup)
mongo_url = os.environ['MONGO_URL']
client = AsyncIOMotorClient(mongo_url)
db = client[os.environ['DB_NAME']]

# Create the main app without a prefix
app = FastAPI(title="IoT Buoy Dashboard API", version="1.0.0")

# Create a router with the /api prefix
api_router = APIRouter(prefix="/api")

# Firebase configuration (placeholder - user will configure)
FIREBASE_CONFIG = {
    "type": "service_account",
    "project_id": "YOUR_PROJECT_ID",
    "private_key_id": "YOUR_PRIVATE_KEY_ID",
    "private_key": "YOUR_PRIVATE_KEY",
    "client_email": "YOUR_CLIENT_EMAIL",
    "client_id": "YOUR_CLIENT_ID",
    "auth_uri": "https://accounts.google.com/o/oauth2/auth",
    "token_uri": "https://oauth2.googleapis.com/token",
    "auth_provider_x509_cert_url": "https://www.googleapis.com/oauth2/v1/certs",
    "client_x509_cert_url": "YOUR_CLIENT_CERT_URL",
    "universe_domain": "googleapis.com"
}

# Initialize Firebase (commented out until configured)
firebase_app = None
if FIREBASE_AVAILABLE and os.getenv("FIREBASE_CONFIGURED", "false").lower() == "true":
    try:
        # Uncomment and configure when ready:
        # cred = credentials.Certificate(FIREBASE_CONFIG)
        # firebase_app = firebase_admin.initialize_app(cred, {
        #     'databaseURL': 'https://your-project-id-default-rtdb.firebaseio.com/'
        # })
        pass
    except Exception as e:
        logging.error(f"Firebase initialization failed: {e}")

# Pydantic Models
class BuoyReading(BaseModel):
    id: str = Field(default_factory=lambda: str(uuid.uuid4()))
    timestamp: datetime = Field(default_factory=lambda: datetime.now(timezone.utc))
    gps_latitude: float
    gps_longitude: float
    battery_percentage: float
    water_turbidity: float  # NTU (Nephelometric Turbidity Units)
    water_temperature: float  # Celsius
    humidity: float  # Percentage
    air_pressure: float  # hPa
    detected_object_class: Optional[str] = None

class BuoyReadingCreate(BaseModel):
    gps_latitude: float
    gps_longitude: float
    battery_percentage: float = Field(ge=0, le=100)
    water_turbidity: float = Field(ge=0)
    water_temperature: float
    humidity: float = Field(ge=0, le=100)
    air_pressure: float = Field(ge=0)
    detected_object_class: Optional[str] = None

class BuoyStatus(BaseModel):
    is_online: bool
    last_reading: Optional[datetime] = None
    connection_quality: str  # "excellent", "good", "poor", "offline"
    total_readings: int

class HistoricalDataQuery(BaseModel):
    start_date: Optional[datetime] = None
    end_date: Optional[datetime] = None
    limit: int = Field(default=168, ge=1, le=1000)  # Default: 7 days * 24 hours

# Helper functions
def prepare_for_mongo(data: dict) -> dict:
    """Prepare data for MongoDB storage by converting datetime objects."""
    if isinstance(data.get('timestamp'), datetime):
        data['timestamp'] = data['timestamp'].isoformat()
    return data

def parse_from_mongo(item: dict) -> dict:
    """Parse data from MongoDB by converting ISO strings back to datetime."""
    if isinstance(item.get('timestamp'), str):
        try:
            item['timestamp'] = datetime.fromisoformat(item['timestamp'].replace('Z', '+00:00'))
        except:
            pass
    return item

async def get_firebase_data(path: str) -> Optional[Dict[Any, Any]]:
    """Get data from Firebase Realtime Database."""
    if not firebase_app:
        return None
    try:
        ref = firebase_db.reference(path, app=firebase_app)
        return ref.get()
    except Exception as e:
        logging.error(f"Firebase read error: {e}")
        return None

async def set_firebase_data(path: str, data: dict) -> bool:
    """Set data to Firebase Realtime Database."""
    if not firebase_app:
        return False
    try:
        ref = firebase_db.reference(path, app=firebase_app)
        ref.set(data)
        return True
    except Exception as e:
        logging.error(f"Firebase write error: {e}")
        return False

# API Routes
@api_router.get("/")
async def root():
    return {
        "message": "IoT Buoy Dashboard API",
        "version": "1.0.0",
        "firebase_status": "configured" if firebase_app else "not_configured"
    }

@api_router.get("/buoy/status", response_model=BuoyStatus)
async def get_buoy_status():
    """Get current buoy status and connection info."""
    try:
        # Count total readings
        total_readings = await db.buoy_readings.count_documents({})
        
        # Get latest reading
        latest_reading = await db.buoy_readings.find_one(
            {}, sort=[("timestamp", -1)]
        )
        
        last_reading = None
        is_online = False
        connection_quality = "offline"
        
        if latest_reading:
            latest_reading = parse_from_mongo(latest_reading)
            last_reading = latest_reading.get('timestamp')
            
            # Determine if buoy is online (reading within last 10 minutes)
            if last_reading:
                time_diff = datetime.now(timezone.utc) - last_reading
                if time_diff.total_seconds() < 600:  # 10 minutes
                    is_online = True
                    if time_diff.total_seconds() < 120:  # 2 minutes
                        connection_quality = "excellent"
                    elif time_diff.total_seconds() < 300:  # 5 minutes
                        connection_quality = "good"
                    else:
                        connection_quality = "poor"
        
        return BuoyStatus(
            is_online=is_online,
            last_reading=last_reading,
            connection_quality=connection_quality,
            total_readings=total_readings
        )
    except Exception as e:
        logging.error(f"Error getting buoy status: {e}")
        raise HTTPException(status_code=500, detail="Failed to get buoy status")

@api_router.post("/buoy/readings", response_model=BuoyReading)
async def create_buoy_reading(reading: BuoyReadingCreate):
    """Create a new buoy sensor reading."""
    try:
        # Create reading object
        reading_obj = BuoyReading(**reading.dict())
        
        # Store in MongoDB
        reading_dict = prepare_for_mongo(reading_obj.dict())
        await db.buoy_readings.insert_one(reading_dict)
        
        # Sync to Firebase if available
        if firebase_app:
            firebase_data = {
                "gps": {
                    "lat": reading.gps_latitude,
                    "lng": reading.gps_longitude
                },
                "battery": reading.battery_percentage,
                "turbidity": reading.water_turbidity,
                "temperature": reading.water_temperature,
                "humidity": reading.humidity,
                "pressure": reading.air_pressure,
                "objectClass": reading.detected_object_class or "unknown",
                "timestamp": reading_obj.timestamp.isoformat()
            }
            
            # Update current reading
            await set_firebase_data('buoy_data/current', firebase_data)
            
            # Add to historical data
            reading_id = reading_obj.id.replace('-', '')
            await set_firebase_data(f'buoy_data/historical/{reading_id}', firebase_data)
        
        return reading_obj
    
    except Exception as e:
        logging.error(f"Error creating buoy reading: {e}")
        raise HTTPException(status_code=500, detail="Failed to create reading")

@api_router.get("/buoy/readings/latest", response_model=BuoyReading)
async def get_latest_reading():
    """Get the most recent buoy reading."""
    try:
        reading = await db.buoy_readings.find_one(
            {}, sort=[("timestamp", -1)]
        )
        
        if not reading:
            raise HTTPException(status_code=404, detail="No readings found")
        
        reading = parse_from_mongo(reading)
        return BuoyReading(**reading)
    
    except HTTPException:
        raise
    except Exception as e:
        logging.error(f"Error getting latest reading: {e}")
        raise HTTPException(status_code=500, detail="Failed to get latest reading")

@api_router.get("/buoy/readings", response_model=List[BuoyReading])
async def get_readings(
    limit: int = 100,
    skip: int = 0,
    start_date: Optional[str] = None,
    end_date: Optional[str] = None
):
    """Get historical buoy readings with optional date filtering."""
    try:
        # Build query filter
        query_filter = {}
        
        if start_date:
            start_dt = datetime.fromisoformat(start_date)
            query_filter["timestamp"] = {"$gte": start_dt.isoformat()}
        
        if end_date:
            end_dt = datetime.fromisoformat(end_date)
            if "timestamp" in query_filter:
                query_filter["timestamp"]["$lte"] = end_dt.isoformat()
            else:
                query_filter["timestamp"] = {"$lte": end_dt.isoformat()}
        
        # Query database
        readings = await db.buoy_readings.find(query_filter)\
            .sort("timestamp", -1)\
            .skip(skip)\
            .limit(limit)\
            .to_list(length=limit)
        
        # Parse readings
        parsed_readings = [parse_from_mongo(reading) for reading in readings]
        return [BuoyReading(**reading) for reading in parsed_readings]
    
    except Exception as e:
        logging.error(f"Error getting readings: {e}")
        raise HTTPException(status_code=500, detail="Failed to get readings")

@api_router.get("/buoy/readings/summary")
async def get_readings_summary(hours: int = 24):
    """Get summarized statistics for recent readings."""
    try:
        # Calculate time threshold
        threshold = datetime.now(timezone.utc) - datetime.timedelta(hours=hours)
        
        # Query recent readings
        readings = await db.buoy_readings.find({
            "timestamp": {"$gte": threshold.isoformat()}
        }).to_list(length=1000)
        
        if not readings:
            return {
                "period_hours": hours,
                "total_readings": 0,
                "summary": {}
            }
        
        # Calculate statistics
        temps = [r.get('water_temperature', 0) for r in readings]
        turbidities = [r.get('water_turbidity', 0) for r in readings]
        batteries = [r.get('battery_percentage', 0) for r in readings]
        humidities = [r.get('humidity', 0) for r in readings]
        pressures = [r.get('air_pressure', 0) for r in readings]
        
        summary = {
            "temperature": {
                "avg": sum(temps) / len(temps) if temps else 0,
                "min": min(temps) if temps else 0,
                "max": max(temps) if temps else 0
            },
            "turbidity": {
                "avg": sum(turbidities) / len(turbidities) if turbidities else 0,
                "min": min(turbidities) if turbidities else 0,
                "max": max(turbidities) if turbidities else 0
            },
            "battery": {
                "avg": sum(batteries) / len(batteries) if batteries else 0,
                "min": min(batteries) if batteries else 0,
                "current": batteries[-1] if batteries else 0
            },
            "humidity": {
                "avg": sum(humidities) / len(humidities) if humidities else 0,
                "min": min(humidities) if humidities else 0,
                "max": max(humidities) if humidities else 0
            },
            "pressure": {
                "avg": sum(pressures) / len(pressures) if pressures else 0,
                "min": min(pressures) if pressures else 0,
                "max": max(pressures) if pressures else 0
            }
        }
        
        return {
            "period_hours": hours,
            "total_readings": len(readings),
            "summary": summary
        }
    
    except Exception as e:
        logging.error(f"Error getting summary: {e}")
        raise HTTPException(status_code=500, detail="Failed to get summary")

@api_router.delete("/buoy/readings")
async def clear_all_readings():
    """Clear all buoy readings (use with caution)."""
    try:
        result = await db.buoy_readings.delete_many({})
        return {"deleted_count": result.deleted_count}
    except Exception as e:
        logging.error(f"Error clearing readings: {e}")
        raise HTTPException(status_code=500, detail="Failed to clear readings")

# Original status check endpoints (keeping for compatibility)
class StatusCheck(BaseModel):
    id: str = Field(default_factory=lambda: str(uuid.uuid4()))
    client_name: str
    timestamp: datetime = Field(default_factory=lambda: datetime.now(timezone.utc))

class StatusCheckCreate(BaseModel):
    client_name: str

@api_router.post("/status", response_model=StatusCheck)
async def create_status_check(input: StatusCheckCreate):
    status_dict = input.dict()
    status_obj = StatusCheck(**status_dict)
    status_data = prepare_for_mongo(status_obj.dict())
    await db.status_checks.insert_one(status_data)
    return status_obj

@api_router.get("/status", response_model=List[StatusCheck])
async def get_status_checks():
    status_checks = await db.status_checks.find().to_list(1000)
    parsed_checks = [parse_from_mongo(check) for check in status_checks]
    return [StatusCheck(**check) for check in parsed_checks]

# Include the router in the main app
app.include_router(api_router)

# CORS middleware
app.add_middleware(
    CORSMiddleware,
    allow_credentials=True,
    allow_origins=os.environ.get('CORS_ORIGINS', '*').split(','),
    allow_methods=["*"],
    allow_headers=["*"],
)

# Configure logging
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(name)s - %(levelname)s - %(message)s'
)
logger = logging.getLogger(__name__)

@app.on_event("shutdown")
async def shutdown_db_client():
    client.close()

if __name__ == "__main__":
    import uvicorn
    uvicorn.run(app, host="0.0.0.0", port=8001)