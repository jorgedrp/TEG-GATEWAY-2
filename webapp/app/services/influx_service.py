import logging
from typing import Dict, Any, Optional
import pandas as pd
from influxdb_client import InfluxDBClient
from webapp.config import Config
from webapp.app.services.dsp_engine import SpectralAnalyzer, KalmanFilter6DoF

logger = logging.getLogger(__name__)

class InfluxService:
    """Servicio cliente para InfluxDB v2."""

    def __init__(self):
        self.url = Config.INFLUX_URL
        self.token = Config.INFLUX_TOKEN
        self.org = Config.INFLUX_ORG
        self.bucket = Config.INFLUX_BUCKET
        self._client: Optional[InfluxDBClient] = None
        self._init_client()

    def _init_client(self):
        try:
            self._client = InfluxDBClient(
                url=self.url,
                token=self.token,
                org=self.org,
                timeout=15_000
            )
        except Exception as e:
            logger.error(f"Error inicializando cliente InfluxDB: {e}")
            self._client = None

    def is_connected(self) -> bool:
        """Verifica la conectividad con el servidor InfluxDB."""
        if not self._client:
            return False
        try:
            return self._client.ping()
        except Exception:
            return False

    def query_telemetry(self, start_time: str = '-5m', stop_time: str = 'now()',
                        channel: str = 'ax', sensor_id: str = '16') -> Dict[str, Any]:
        """
        Consulta las mediciones de aceleración MPU6050 y ambientales BME280.
        Calcula el espectro FFT para el canal especificado.
        """
        if not self._client:
            self._init_client()
            if not self._client:
                raise RuntimeError("No hay conexión con InfluxDB.")

        try:
            sensor_base = int(sensor_id)
            accel_id = str(sensor_base + 1)
            amb_id = str(sensor_base + 2)
        except ValueError:
            raise ValueError("El ID de sensor debe ser un número entero válido.")

        flux_query = f'''
        from(bucket: "{self.bucket}")
            |> range(start: {start_time}, stop: {stop_time})
            |> filter(fn: (r) => r["_measurement"] == "bme280" or r["_measurement"] == "mpu6050")
            |> filter(fn: (r) => r["_field"] == "{channel}" or r["_field"] == "humedad" or r["_field"] == "presion" or r["_field"] == "temperatura")
            |> filter(fn: (r) => r["device_id"] == "{accel_id}" or r["device_id"] == "{amb_id}")
        '''

        query_api = self._client.query_api()
        result = query_api.query(flux_query)

        accel_times = []
        accel_values = []
        amb_data_dict = {}

        for table in result:
            for record in table.records:
                dev_id = record.values.get("device_id")
                time_obj = record.get_time()
                field = record.get_field()
                val = record.get_value()

                if dev_id == accel_id and field == channel and val is not None:
                    accel_times.append(time_obj)
                    accel_values.append(float(val))
                elif dev_id == amb_id and val is not None:
                    if time_obj not in amb_data_dict:
                        amb_data_dict[time_obj] = {}
                    amb_data_dict[time_obj][field] = float(val)

        if not accel_values:
            raise LookupError(f"No se encontraron datos para el canal '{channel}' en el rango especificado.")

        # Cálculo de FFT
        xf, mag, peak_freq = SpectralAnalyzer.compute_fft(accel_values, accel_times)

        # Formato de Timestamps locales con milisegundos
        accel_timestamps_formatted = [
            f"{t.astimezone(Config.TIMEZONE).strftime('%H:%M:%S')}:{int(t.microsecond / 1000):03d}"
            for t in accel_times
        ]

        # Datos ambientales ordenados
        sorted_amb_times = sorted(amb_data_dict.keys())
        amb_times_formatted = []
        temp_values = []
        hum_values = []
        pres_values = []

        for t_obj in sorted_amb_times:
            amb_times_formatted.append(t_obj.astimezone(Config.TIMEZONE).strftime('%H:%M:%S'))
            temp_values.append(amb_data_dict[t_obj].get("temperatura"))
            hum_values.append(amb_data_dict[t_obj].get("humedad"))
            pres_values.append(amb_data_dict[t_obj].get("presion"))

        accel_epoch = [t.timestamp() for t in accel_times]
        amb_epoch = [t_obj.timestamp() for t_obj in sorted_amb_times]

        return {
            "timeSeries": {
                "time": accel_timestamps_formatted,
                "timestamps_epoch": accel_epoch,
                "data": accel_values
            },
            "frequencySeries": {
                "frequencies": xf,
                "magnitude": mag,
                "peak_frequency": peak_freq
            },
            "ambSeries": {
                "time": amb_times_formatted,
                "timestamps_epoch": amb_epoch,
                "temp": temp_values,
                "hum": hum_values,
                "pres": pres_values
            }
        }

    def query_inclinacion(self, start_time: str = '-5m', stop_time: str = 'now()',
                          sensor_id: str = '16') -> Dict[str, Any]:
        """Consulta 6-DoF y ejecuta el filtro de Kalman para estimar Roll y Pitch."""
        if not self._client:
            self._init_client()
            if not self._client:
                raise RuntimeError("No hay conexión con InfluxDB.")

        try:
            sensor_base = int(sensor_id)
            accel_id = str(sensor_base + 1)
        except ValueError:
            raise ValueError("El ID de sensor debe ser un número entero válido.")

        flux_query = f'''
        from(bucket: "{self.bucket}")
          |> range(start: {start_time}, stop: {stop_time}) 
          |> filter(fn: (r) => r["_measurement"] == "mpu6050")
          |> filter(fn: (r) => r["device_id"] == "{accel_id}")
          |> filter(fn: (r) => r["_field"] == "ax" or r["_field"] == "ay" or r["_field"] == "az" or r["_field"] == "gx" or r["_field"] == "gy" or r["_field"] == "gz")
          |> pivot(rowKey:["_time"], columnKey: ["_field"], valueColumn: "_value")
          |> keep(columns: ["_time", "ax", "ay", "az", "gx", "gy", "gz"])
        '''

        query_api = self._client.query_api()
        df = query_api.query_data_frame(flux_query)

        if df is None or (isinstance(df, pd.DataFrame) and df.empty):
            raise LookupError("No se encontraron datos 6-DoF en el rango de tiempo seleccionado.")

        return KalmanFilter6DoF.process_dataframe(df)

# Singleton global del servicio InfluxDB
influx_service = InfluxService()
