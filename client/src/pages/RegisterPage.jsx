import { useState } from 'react';
import { useNavigate, Link } from 'react-router-dom';
import api from '../api/client';
import { useAuth } from '../context/AuthContext';

export default function RegisterPage() {
  const { login } = useAuth();
  const navigate = useNavigate();
  const [username, setUsername] = useState('');
  const [email, setEmail] = useState('');
  const [password, setPassword] = useState('');
  const [error, setError] = useState('');
  const [loading, setLoading] = useState(false);

  const handleRegister = async () => {
    setError('');
    if (!username || !email || !password) { setError('All fields are required.'); return; }
    setLoading(true);
    try {
      const { data } = await api.post('/api/auth/register', { username, email, password });
      login(data.token, data.username);
      navigate('/dashboard');
    } catch (err) {
      setError(err.response?.data?.error || 'Registration failed. Please try again.');
    } finally {
      setLoading(false);
    }
  };

  return (
    <div style={s.page}>
      <div style={s.card}>
        <h1 style={s.title}>Create Account</h1>
        <p style={s.sub}>ESP32 Trivia Platform</p>

        {error && <div style={s.error}>{error}</div>}

        <div style={s.field}>
          <label style={s.label}>Username</label>
          <input
            style={s.input}
            type="text"
            value={username}
            placeholder="coolplayer42"
            onChange={(e) => setUsername(e.target.value)}
            onKeyDown={(e) => e.key === 'Enter' && handleRegister()}
          />
        </div>

        <div style={s.field}>
          <label style={s.label}>Email</label>
          <input
            style={s.input}
            type="email"
            value={email}
            placeholder="you@example.com"
            onChange={(e) => setEmail(e.target.value)}
            onKeyDown={(e) => e.key === 'Enter' && handleRegister()}
          />
        </div>

        <div style={s.field}>
          <label style={s.label}>Password</label>
          <input
            style={s.input}
            type="password"
            value={password}
            placeholder="••••••••"
            onChange={(e) => setPassword(e.target.value)}
            onKeyDown={(e) => e.key === 'Enter' && handleRegister()}
          />
        </div>

        <div style={s.btn} onClick={!loading ? handleRegister : undefined}>
          {loading ? 'Creating account…' : 'Create Account'}
        </div>

        <p style={s.footer}>
          Already have an account?{' '}
          <Link to="/login" style={s.link}>Sign in</Link>
        </p>
      </div>
    </div>
  );
}

const s = {
  page: { minHeight: '100vh', display: 'flex', alignItems: 'center', justifyContent: 'center', background: '#f0f2f5' },
  card: { background: '#fff', borderRadius: 12, padding: '44px 48px', boxShadow: '0 2px 20px rgba(0,0,0,0.10)', width: 360 },
  title: { margin: '0 0 4px', fontSize: 28, fontWeight: 700, color: '#1a1a2e' },
  sub: { margin: '0 0 28px', color: '#888', fontSize: 14 },
  error: { background: '#fde8e8', color: '#c0392b', borderRadius: 8, padding: '10px 14px', marginBottom: 20, fontSize: 14 },
  field: { marginBottom: 18 },
  label: { display: 'block', marginBottom: 6, fontSize: 13, fontWeight: 600, color: '#444' },
  input: { width: '100%', padding: '10px 12px', borderRadius: 8, border: '1.5px solid #ddd', fontSize: 15, outline: 'none', boxSizing: 'border-box' },
  btn: { marginTop: 8, width: '100%', padding: '12px 0', background: '#4f46e5', color: '#fff', borderRadius: 8, fontSize: 16, fontWeight: 600, textAlign: 'center', cursor: 'pointer', userSelect: 'none' },
  footer: { marginTop: 20, textAlign: 'center', fontSize: 14, color: '#666' },
  link: { color: '#4f46e5', textDecoration: 'none', fontWeight: 600 },
};
