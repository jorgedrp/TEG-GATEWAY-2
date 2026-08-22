import math
from typing import List, Tuple, Dict, Any, Optional
import numpy as np
import pandas as pd
from scipy.signal import get_window
from webapp.config import Config

class KalmanAngle:
    """Filtro de Kalman de 2 estados para estimación de inclinación (Roll o Pitch)."""

    def __init__(self, Q_angle: float = 0.001, Q_bias: float = 0.003, R_measure: float = 0.03):
        """
        Q_angle:   Varianza del proceso para el ángulo (acelerómetro/giroscopio)
        Q_bias:    Varianza del proceso para la deriva del sesgo del giroscopio
        R_measure: Varianza del ruido de medición (vibraciones en el acelerómetro)
        """
        self.Q_angle = Q_angle
        self.Q_bias = Q_bias
        self.R_measure = R_measure
        self.angle = 0.0
        self.bias = 0.0
        self.rate = 0.0
        self.P = [[0.0, 0.0], [0.0, 0.0]]

    def compute(self, new_angle: float, new_rate: float, dt: float) -> float:
        """
        new_angle: Ángulo calculado trigonométricamente con acelerómetro (grados)
        new_rate:  Velocidad angular cruda del giroscopio (grados/s)
        dt:        Paso de tiempo transcurrido (segundos)
        """
        if dt <= 0:
            dt = 0.01

        # 1. PREDICCIÓN (Modelo dinámico del estado)
        self.rate = new_rate - self.bias
        self.angle += self.rate * dt

        self.P[0][0] += dt * (dt * self.P[1][1] - self.P[0][1] - self.P[1][0] + self.Q_angle)
        self.P[0][1] -= dt * self.P[1][1]
        self.P[1][0] -= dt * self.P[1][1]
        self.P[1][1] += self.Q_bias * dt

        # 2. ACTUALIZACIÓN / CORRECCIÓN (Medición del acelerómetro)
        y = new_angle - self.angle
        S = self.P[0][0] + self.R_measure
        K = [self.P[0][0] / S, self.P[1][0] / S]

        self.angle += K[0] * y
        self.bias += K[1] * y

        p00_temp = self.P[0][0]
        p01_temp = self.P[0][1]
        self.P[0][0] -= K[0] * p00_temp
        self.P[0][1] -= K[0] * p01_temp
        self.P[1][0] -= K[1] * p00_temp
        self.P[1][1] -= K[1] * p01_temp

        return self.angle

class KalmanFilter6DoF:
    """Fusión sensorial 6-DoF para cálculo de inclinación estructural Roll & Pitch."""

    @staticmethod
    def process_dataframe(df: pd.DataFrame, resample_rule: str = '1s') -> Dict[str, Any]:
        if df.empty:
            return {"labels": [], "roll": [], "pitch": []}

        # Asegurar ordenación temporal
        if not isinstance(df.index, pd.DatetimeIndex):
            df['_time'] = pd.to_datetime(df['_time'])
            df.set_index('_time', inplace=True)
        df.sort_index(inplace=True)

        kalman_x = KalmanAngle()  # Roll
        kalman_y = KalmanAngle()  # Pitch

        roll_list = []
        pitch_list = []

        # dt dinámico entre muestras
        dt_series = df.index.to_series().diff().dt.total_seconds().fillna(0.01)

        for i, row in enumerate(df.itertuples()):
            ax, ay, az = row.ax, row.ay, row.az
            gx, gy, gz = row.gx, row.gy, row.gz
            dt = float(dt_series.iloc[i])
            if dt <= 0:
                dt = 0.01

            # Trigonometría (Roll = atan2(Y, Z), Pitch = atan2(-X, sqrt(Y^2 + Z^2)))
            roll_acc = math.atan2(ay, az) * 180.0 / math.pi
            pitch_acc = math.atan2(-ax, math.sqrt(ay * ay + az * az)) * 180.0 / math.pi

            roll_k = kalman_x.compute(roll_acc, gx, dt)
            pitch_k = kalman_y.compute(pitch_acc, gy, dt)

            roll_list.append(roll_k)
            pitch_list.append(pitch_k)

        df['roll_kalman'] = roll_list
        df['pitch_kalman'] = pitch_list

        # Downsampling para visualización fluida
        df_final = df[['roll_kalman', 'pitch_kalman']].resample(resample_rule).mean().dropna()

        # Ajuste de zona horaria
        if df_final.index.tz is None:
            df_final.index = df_final.index.tz_localize('UTC').tz_convert(Config.TIMEZONE)
        else:
            df_final.index = df_final.index.tz_convert(Config.TIMEZONE)

        labels = df_final.index.strftime('%H:%M:%S').tolist()
        timestamps_epoch = [t.timestamp() for t in df_final.index]
        roll_data = df_final['roll_kalman'].round(2).tolist()
        pitch_data = df_final['pitch_kalman'].round(2).tolist()

        return {
            "labels": labels,
            "timestamps_epoch": timestamps_epoch,
            "roll": roll_data,
            "pitch": pitch_data
        }

class SpectralAnalyzer:
    """Motor de Procesamiento Espectral (FFT con ventaneo Hann y corrección de ganancia coherente ACF)."""

    @staticmethod
    def compute_fft(values: List[float], timestamps: List[Any]) -> Tuple[List[float], List[float], Optional[float]]:
        """
        Calcula el espectro unilateral de magnitud con ventana Hann y factor ACF.
        Devuelve: (frecuencias, magnitudes, frecuencia_pico_hz)
        """
        if len(values) < 2:
            return [], [], None

        data_array = np.array(values, dtype=float)
        # 1. Eliminación de offset DC (gravedad)
        mean_val = np.mean(data_array)
        mean_subtracted = data_array - mean_val

        # 2. Ventana de Hann
        n_samples = len(mean_subtracted)
        window = get_window('hann', n_samples)
        windowed_data = mean_subtracted * window

        # 3. Duración y frecuencia de muestreo efectiva
        t_start = timestamps[0]
        t_end = timestamps[-1]
        if hasattr(t_start, 'timestamp') and hasattr(t_end, 'timestamp'):
            duration = t_end.timestamp() - t_start.timestamp()
        else:
            duration = (pd.to_datetime(t_end) - pd.to_datetime(t_start)).total_seconds()

        if duration <= 0:
            duration = n_samples * 0.005 # Fallback a ~200Hz

        fs = (n_samples - 1) / duration

        # 4. RFFT y factor de corrección de amplitud (ACF)
        yf = np.fft.rfft(windowed_data)
        xf = np.fft.rfftfreq(n_samples, 1.0 / fs)

        sum_window = np.sum(window)
        acf = (n_samples / sum_window) if sum_window > 0 else 1.0

        magnitude = (2.0 / n_samples) * acf * np.abs(yf)
        magnitude[0] = magnitude[0] / 2.0
        if n_samples % 2 == 0 and len(magnitude) > 1:
            magnitude[-1] = magnitude[-1] / 2.0

        freqs_list = [round(float(f), 3) for f in xf.tolist()]
        mags_list = [round(float(m), 5) for m in magnitude.tolist()]

        # 5. Detección de frecuencia resonante principal (ignorando DC)
        peak_freq = None
        if len(mags_list) > 1:
            peak_idx = int(np.argmax(magnitude[1:])) + 1
            peak_freq = round(float(xf[peak_idx]), 2)

        return freqs_list, mags_list, peak_freq
