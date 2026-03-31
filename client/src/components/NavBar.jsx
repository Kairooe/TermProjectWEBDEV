import { Link, useNavigate } from 'react-router-dom';
import { useAuth } from '../context/AuthContext';

export default function NavBar() {
  const { logout } = useAuth();
  const navigate = useNavigate();

  const handleLogout = () => {
    logout();
    navigate('/login');
  };

  return (
    <nav style={s.nav}>
      <div style={s.brand}>ESP32 Trivia</div>
      <div style={s.links}>
        <Link to="/dashboard" style={s.link}>Dashboard</Link>
        <Link to="/profile" style={s.link}>Profile</Link>
        <Link to="/leaderboard" style={s.link}>Leaderboard</Link>
        <span style={s.logout} onClick={handleLogout}>Logout</span>
      </div>
    </nav>
  );
}

const s = {
  nav: { display: 'flex', alignItems: 'center', justifyContent: 'space-between', padding: '0 32px', height: 56, background: '#1a1a2e', position: 'sticky', top: 0, zIndex: 100 },
  brand: { color: '#fff', fontWeight: 700, fontSize: 18, letterSpacing: 0.5 },
  links: { display: 'flex', gap: 28, alignItems: 'center' },
  link: { color: '#c7c7e0', textDecoration: 'none', fontSize: 14, fontWeight: 500 },
  logout: { color: '#e57373', fontSize: 14, fontWeight: 500, cursor: 'pointer', userSelect: 'none' },
};
