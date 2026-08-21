import math
import numpy as np
import pandas as pd
from webapp.app.services.dsp_engine import KalmanAngle, KalmanFilter6DoF, SpectralAnalyzer

def test_kalman_angle_convergence():
    """Verifica que el filtro de Kalman converge hacia el ángulo medido por acelerómetro."""
    kalman = KalmanAngle(Q_angle=0.001, Q_bias=0.003, R_measure=0.03)
    
    true_angle = 15.0  # 15 grados constantes
    gyro_rate = 0.0    # Sin rotación
    dt = 0.01

    # Simular 500 iteraciones (5 segundos a 100 Hz) con ruido gaussiano
    np.random.seed(42)
    estimated_angles = []
    for _ in range(500):
        noisy_measurement = true_angle + np.random.normal(0, 0.5)
        angle = kalman.compute(noisy_measurement, gyro_rate, dt)
        estimated_angles.append(angle)

    # Al final de las iteraciones el filtro debe converger al valor real
    final_error = abs(estimated_angles[-1] - true_angle)
    assert final_error < 0.3, f"Error final {final_error} es mayor al umbral esperado."

def test_spectral_analyzer_pure_sine():
    """Verifica que la FFT identifica correctamente la frecuencia dominante de una onda senoidal pura."""
    fs = 200.0  # 200 Hz
    duration = 2.0  # 2 segundos
    f_signal = 12.5  # Frecuencia senoidal conocida: 12.5 Hz
    
    n_samples = int(fs * duration)
    t = np.linspace(0, duration, n_samples, endpoint=False)
    signal = 2.5 * np.sin(2 * np.pi * f_signal * t) + 1.0  # Incluye offset DC de 1.0

    # Crear lista de marcas de tiempo
    base_time = pd.Timestamp('2026-08-20 12:00:00')
    timestamps = [base_time + pd.Timedelta(seconds=float(ti)) for ti in t]

    freqs, mags, peak_freq = SpectralAnalyzer.compute_fft(signal.tolist(), timestamps)

    assert len(freqs) > 0
    assert len(mags) == len(freqs)
    assert peak_freq is not None
    # El pico debe coincidir con 12.5 Hz dentro de una resolución razonable
    assert abs(peak_freq - f_signal) < 0.5, f"Pico detectado {peak_freq} difiere de {f_signal} Hz"

def test_kalman_filter_dataframe():
    """Verifica que el método batch process_dataframe procesa correctamente un DataFrame."""
    n_rows = 50
    dates = pd.date_range('2026-08-20 12:00:00', periods=n_rows, freq='100ms')
    df = pd.DataFrame({
        '_time': dates,
        'ax': np.zeros(n_rows),
        'ay': np.zeros(n_rows),
        'az': np.ones(n_rows) * 9.81,
        'gx': np.zeros(n_rows),
        'gy': np.zeros(n_rows),
        'gz': np.zeros(n_rows)
    })

    result = KalmanFilter6DoF.process_dataframe(df)
    assert "labels" in result
    assert "roll" in result
    assert "pitch" in result
    assert len(result["roll"]) > 0
    # En reposo con Z=9.81, Roll y Pitch deben ser cercanos a 0 grados
    assert abs(result["roll"][0]) < 1.0
    assert abs(result["pitch"][0]) < 1.0
