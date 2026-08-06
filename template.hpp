#ifndef NOROI_TEMPLATE_HPP_
#define NOROI_TEMPLATE_HPP_

namespace noroi::templates {

inline constexpr char kWelcome[] = R"mustache(<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <meta name="description" content="{{meta_description}}">
  <title>{{page_title}}</title>
  <style>
    :root {
      color-scheme: dark;
      --ink: #f7f6f2;
      --muted: #a6a9b8;
      --panel: rgba(17, 19, 29, 0.82);
      --panel-strong: #151823;
      --line: rgba(255, 255, 255, 0.1);
      --violet: #9b87f5;
      --violet-soft: #c9bfff;
      --cyan: #69e6d4;
      --amber: #ffc970;
      --danger: #ff7d8a;
      --shadow: 0 28px 80px rgba(0, 0, 0, 0.38);
    }

    * {
      box-sizing: border-box;
    }

    html {
      scroll-behavior: smooth;
    }

    body {
      min-width: 320px;
      margin: 0;
      overflow-x: hidden;
      color: var(--ink);
      background:
        radial-gradient(circle at 8% 6%, rgba(105, 230, 212, 0.12), transparent 30rem),
        radial-gradient(circle at 88% 8%, rgba(155, 135, 245, 0.2), transparent 34rem),
        #0a0c12;
      font-family: Inter, ui-sans-serif, system-ui, -apple-system, BlinkMacSystemFont,
        "Segoe UI", sans-serif;
      line-height: 1.6;
    }

    a {
      color: inherit;
      text-decoration: none;
    }

    button,
    a {
      -webkit-tap-highlight-color: transparent;
    }

    .ambient {
      position: fixed;
      z-index: -1;
      width: 24rem;
      height: 24rem;
      border-radius: 999px;
      filter: blur(90px);
      opacity: 0.13;
      pointer-events: none;
    }

    .ambient-one {
      top: 38rem;
      left: -12rem;
      background: var(--violet);
    }

    .ambient-two {
      right: -12rem;
      bottom: 18rem;
      background: var(--cyan);
    }

    .shell {
      width: min(1160px, calc(100% - 2rem));
      margin-inline: auto;
    }

    .site-header {
      display: flex;
      align-items: center;
      justify-content: space-between;
      min-height: 78px;
      gap: 1.5rem;
      border-bottom: 1px solid var(--line);
    }

    .brand {
      display: inline-flex;
      align-items: center;
      gap: 0.75rem;
      flex-shrink: 0;
    }

    .brand-mark {
      display: grid;
      width: 2.4rem;
      height: 2.4rem;
      place-items: center;
      border: 1px solid rgba(255, 255, 255, 0.2);
      border-radius: 0.8rem;
      background: linear-gradient(145deg, rgba(155, 135, 245, 0.9), rgba(105, 230, 212, 0.7));
      box-shadow: inset 0 1px rgba(255, 255, 255, 0.3), 0 10px 30px rgba(105, 230, 212, 0.12);
      color: #090b10;
      font-weight: 900;
    }

    .brand-copy {
      display: grid;
      line-height: 1.15;
    }

    .brand-copy strong {
      letter-spacing: -0.02em;
    }

    .brand-copy small {
      margin-top: 0.2rem;
      color: var(--muted);
      font-size: 0.7rem;
      letter-spacing: 0.12em;
      text-transform: uppercase;
    }

    .nav {
      display: flex;
      align-items: center;
      gap: 0.25rem;
      padding: 0.25rem;
      border: 1px solid var(--line);
      border-radius: 999px;
      background: rgba(255, 255, 255, 0.025);
    }

    .nav a {
      padding: 0.45rem 0.85rem;
      border-radius: 999px;
      color: var(--muted);
      font-size: 0.82rem;
      transition: background 160ms ease, color 160ms ease;
    }

    .nav a:hover,
    .nav a.is-current {
      color: var(--ink);
      background: rgba(255, 255, 255, 0.08);
    }

    .header-link {
      padding: 0.58rem 0.95rem;
      border: 1px solid rgba(201, 191, 255, 0.28);
      border-radius: 0.75rem;
      color: var(--violet-soft);
      font-size: 0.82rem;
      font-weight: 700;
    }

    .hero {
      display: grid;
      grid-template-columns: minmax(0, 0.92fr) minmax(480px, 1.08fr);
      align-items: center;
      gap: clamp(3rem, 7vw, 7rem);
      min-height: 720px;
      padding-block: 5rem 6.5rem;
    }

    .announcement {
      display: inline-flex;
      align-items: center;
      gap: 0.7rem;
      max-width: 100%;
      margin-bottom: 1.5rem;
      padding: 0.42rem 0.72rem 0.42rem 0.45rem;
      border: 1px solid rgba(155, 135, 245, 0.26);
      border-radius: 999px;
      background: rgba(155, 135, 245, 0.07);
      color: #d9d4f7;
      font-size: 0.78rem;
    }

    .announcement-badge {
      padding: 0.15rem 0.45rem;
      border-radius: 999px;
      background: var(--violet);
      color: #0b0c12;
      font-size: 0.68rem;
      font-weight: 900;
      letter-spacing: 0.06em;
      text-transform: uppercase;
    }

    .announcement a {
      color: var(--violet-soft);
      font-weight: 700;
    }

    .eyebrow {
      margin: 0 0 0.9rem;
      color: var(--cyan);
      font-size: 0.78rem;
      font-weight: 800;
      letter-spacing: 0.17em;
      text-transform: uppercase;
    }

    h1,
    h2,
    h3,
    p {
      margin-top: 0;
    }

    h1 {
      max-width: 760px;
      margin-bottom: 1.35rem;
      font-size: clamp(3.3rem, 7.2vw, 6.8rem);
      line-height: 0.92;
      letter-spacing: -0.068em;
    }

    h1 span {
      display: block;
      color: transparent;
      background: linear-gradient(100deg, var(--violet-soft), var(--cyan) 80%);
      -webkit-background-clip: text;
      background-clip: text;
    }

    .hero-copy > p:not(.eyebrow) {
      max-width: 590px;
      color: var(--muted);
      font-size: clamp(1rem, 1.8vw, 1.18rem);
    }

    .hero-actions {
      display: flex;
      flex-wrap: wrap;
      gap: 0.8rem;
      margin-top: 2rem;
    }

    .button {
      display: inline-flex;
      align-items: center;
      justify-content: center;
      min-height: 46px;
      padding: 0.68rem 1rem;
      border: 1px solid var(--line);
      border-radius: 0.85rem;
      font-size: 0.86rem;
      font-weight: 800;
      transition: transform 160ms ease, border-color 160ms ease, background 160ms ease;
    }

    .button:hover {
      transform: translateY(-2px);
    }

    .button-primary {
      border-color: transparent;
      background: var(--ink);
      color: #0c0d12;
      box-shadow: 0 12px 30px rgba(255, 255, 255, 0.1);
    }

    .button-secondary {
      background: rgba(255, 255, 255, 0.035);
      color: var(--ink);
    }

    .trust-row {
      display: flex;
      align-items: center;
      gap: 0.8rem;
      margin-top: 2.4rem;
      color: var(--muted);
      font-size: 0.78rem;
    }

    .avatar-stack {
      display: flex;
    }

    .avatar-stack span {
      display: grid;
      width: 2rem;
      height: 2rem;
      place-items: center;
      margin-left: -0.42rem;
      border: 2px solid #0a0c12;
      border-radius: 999px;
      background: var(--panel-strong);
      color: var(--violet-soft);
      font-size: 0.65rem;
      font-weight: 900;
    }

    .avatar-stack span:first-child {
      margin-left: 0;
    }

    .dashboard-wrap {
      position: relative;
    }

    .dashboard-wrap::before {
      position: absolute;
      inset: 8% -6%;
      z-index: -1;
      border-radius: 50%;
      background: rgba(155, 135, 245, 0.18);
      filter: blur(70px);
      content: "";
    }

    .dashboard {
      overflow: hidden;
      border: 1px solid rgba(255, 255, 255, 0.13);
      border-radius: 1.3rem;
      background: linear-gradient(145deg, rgba(24, 27, 39, 0.96), rgba(13, 15, 23, 0.94));
      box-shadow: var(--shadow);
      transform: perspective(1400px) rotateY(-4deg) rotateX(2deg);
    }

    .dashboard-bar {
      display: flex;
      align-items: center;
      justify-content: space-between;
      padding: 0.9rem 1rem;
      border-bottom: 1px solid var(--line);
      background: rgba(255, 255, 255, 0.02);
    }

    .window-dots {
      display: flex;
      gap: 0.38rem;
    }

    .window-dots i {
      display: block;
      width: 0.58rem;
      height: 0.58rem;
      border-radius: 999px;
      background: rgba(255, 255, 255, 0.18);
    }

    .window-dots i:first-child {
      background: var(--danger);
    }

    .window-dots i:nth-child(2) {
      background: var(--amber);
    }

    .window-dots i:last-child {
      background: var(--cyan);
    }

    .live-status {
      display: inline-flex;
      align-items: center;
      gap: 0.45rem;
      color: var(--muted);
      font-size: 0.7rem;
    }

    .live-status::before {
      width: 0.46rem;
      height: 0.46rem;
      border-radius: 999px;
      background: var(--cyan);
      box-shadow: 0 0 0 4px rgba(105, 230, 212, 0.11);
      content: "";
    }

    .dashboard-body {
      display: grid;
      grid-template-columns: 0.35fr 1fr;
      min-height: 430px;
    }

    .side-rail {
      padding: 1rem 0.75rem;
      border-right: 1px solid var(--line);
      background: rgba(5, 7, 12, 0.3);
    }

    .side-rail span {
      display: block;
      height: 0.55rem;
      margin: 0.65rem 0.3rem;
      border-radius: 999px;
      background: rgba(255, 255, 255, 0.07);
    }

    .side-rail span:first-child,
    .side-rail span:nth-child(4) {
      width: 70%;
      background: rgba(155, 135, 245, 0.25);
    }

    .dashboard-main {
      padding: 1rem;
    }

    .dashboard-heading {
      display: flex;
      align-items: end;
      justify-content: space-between;
      gap: 1rem;
      margin-bottom: 1rem;
    }

    .dashboard-heading small,
    .metric small,
    .activity-copy small {
      color: var(--muted);
      font-size: 0.67rem;
    }

    .dashboard-heading strong {
      display: block;
      margin-top: 0.15rem;
      font-size: 1.08rem;
    }

    .dashboard-heading code {
      padding: 0.25rem 0.45rem;
      border: 1px solid var(--line);
      border-radius: 0.45rem;
      color: var(--cyan);
      background: rgba(105, 230, 212, 0.05);
      font-size: 0.65rem;
    }

    .metric-grid {
      display: grid;
      grid-template-columns: repeat(3, 1fr);
      gap: 0.65rem;
    }

    .metric {
      padding: 0.8rem;
      border: 1px solid var(--line);
      border-radius: 0.8rem;
      background: rgba(255, 255, 255, 0.025);
    }

    .metric strong {
      display: block;
      margin: 0.15rem 0;
      font-size: 1.15rem;
      letter-spacing: -0.04em;
    }

    .metric em {
      color: var(--cyan);
      font-size: 0.65rem;
      font-style: normal;
    }

    .chart {
      position: relative;
      height: 115px;
      margin: 0.8rem 0;
      overflow: hidden;
      border: 1px solid var(--line);
      border-radius: 0.85rem;
      background:
        linear-gradient(rgba(255, 255, 255, 0.035) 1px, transparent 1px),
        linear-gradient(90deg, rgba(255, 255, 255, 0.035) 1px, transparent 1px);
      background-size: 100% 28px, 42px 100%;
    }

    .chart::before {
      position: absolute;
      inset: 20% -10% -35%;
      border: 3px solid var(--violet);
      border-right-color: var(--cyan);
      border-bottom: 0;
      border-left-color: transparent;
      border-radius: 50% 50% 0 0;
      transform: rotate(-4deg);
      content: "";
    }

    .activity-list {
      display: grid;
      gap: 0.5rem;
    }

    .activity {
      display: grid;
      grid-template-columns: auto 1fr auto;
      align-items: center;
      gap: 0.7rem;
      padding: 0.55rem 0.65rem;
      border: 1px solid var(--line);
      border-radius: 0.7rem;
      background: rgba(255, 255, 255, 0.018);
    }

    .activity-marker {
      display: grid;
      width: 1.7rem;
      height: 1.7rem;
      place-items: center;
      border-radius: 0.5rem;
      background: rgba(155, 135, 245, 0.14);
      color: var(--violet-soft);
      font-size: 0.62rem;
      font-weight: 900;
    }

    .activity-marker.tone-cyan {
      color: var(--cyan);
      background: rgba(105, 230, 212, 0.1);
    }

    .activity-marker.tone-amber {
      color: var(--amber);
      background: rgba(255, 201, 112, 0.1);
    }

    .activity-copy strong {
      display: block;
      font-size: 0.72rem;
      line-height: 1.25;
    }

    .activity time {
      color: var(--muted);
      font-size: 0.62rem;
    }

    .stat-band {
      display: grid;
      grid-template-columns: repeat(4, 1fr);
      margin-bottom: 7rem;
      border-block: 1px solid var(--line);
    }

    .stat {
      padding: 2rem clamp(1rem, 3vw, 2.5rem);
      border-right: 1px solid var(--line);
    }

    .stat:last-child {
      border-right: 0;
    }

    .stat strong {
      display: block;
      font-size: clamp(1.8rem, 3.5vw, 2.8rem);
      line-height: 1;
      letter-spacing: -0.05em;
    }

    .stat span {
      display: block;
      margin-top: 0.55rem;
      font-size: 0.82rem;
      font-weight: 750;
    }

    .stat small {
      color: var(--muted);
      font-size: 0.7rem;
    }

    .section-heading {
      display: grid;
      grid-template-columns: minmax(0, 1.15fr) minmax(280px, 0.7fr);
      align-items: end;
      gap: 3rem;
      margin-bottom: 2.5rem;
    }

    .section-heading h2 {
      max-width: 750px;
      margin-bottom: 0;
      font-size: clamp(2.3rem, 5vw, 4.4rem);
      line-height: 1;
      letter-spacing: -0.055em;
    }

    .section-heading > p {
      margin-bottom: 0;
      color: var(--muted);
    }

    .features {
      padding-bottom: 8rem;
    }

    .feature-grid {
      display: grid;
      grid-template-columns: repeat(12, 1fr);
      gap: 1rem;
    }

    .feature-card {
      position: relative;
      grid-column: span 4;
      min-height: 260px;
      padding: 1.4rem;
      overflow: hidden;
      border: 1px solid var(--line);
      border-radius: 1.2rem;
      background: var(--panel);
      box-shadow: inset 0 1px rgba(255, 255, 255, 0.04);
    }

    .feature-card.card-wide {
      grid-column: span 8;
    }

    .feature-card.card-tall {
      min-height: 320px;
    }

    .feature-card::after {
      position: absolute;
      right: -4rem;
      bottom: -5rem;
      width: 12rem;
      height: 12rem;
      border-radius: 999px;
      background: rgba(155, 135, 245, 0.09);
      filter: blur(6px);
      content: "";
    }

    .feature-topline {
      display: flex;
      align-items: center;
      justify-content: space-between;
      gap: 1rem;
    }

    .feature-icon {
      display: grid;
      width: 2.6rem;
      height: 2.6rem;
      place-items: center;
      border: 1px solid rgba(105, 230, 212, 0.2);
      border-radius: 0.8rem;
      color: var(--cyan);
      background: rgba(105, 230, 212, 0.06);
      font-weight: 900;
    }

    .feature-badge {
      padding: 0.18rem 0.45rem;
      border-radius: 999px;
      background: rgba(255, 201, 112, 0.11);
      color: var(--amber);
      font-size: 0.62rem;
      font-weight: 800;
      letter-spacing: 0.08em;
      text-transform: uppercase;
    }

    .feature-card .kicker {
      margin: 2.2rem 0 0.35rem;
      color: var(--violet-soft);
      font-size: 0.68rem;
      font-weight: 800;
      letter-spacing: 0.12em;
      text-transform: uppercase;
    }

    .feature-card h3 {
      margin-bottom: 0.65rem;
      font-size: 1.35rem;
      letter-spacing: -0.03em;
    }

    .feature-card p:last-child {
      max-width: 520px;
      margin-bottom: 0;
      color: var(--muted);
      font-size: 0.85rem;
    }

    .workflow {
      padding-block: 7rem;
      border-block: 1px solid var(--line);
      background: linear-gradient(180deg, rgba(255, 255, 255, 0.018), transparent);
    }

    .step-grid {
      display: grid;
      grid-template-columns: repeat(3, 1fr);
      gap: 1rem;
    }

    .step {
      position: relative;
      padding: 1.5rem;
      border-left: 1px solid var(--line);
    }

    .step-number {
      display: inline-grid;
      width: 2rem;
      height: 2rem;
      place-items: center;
      margin-bottom: 4rem;
      border: 1px solid rgba(155, 135, 245, 0.3);
      border-radius: 999px;
      color: var(--violet-soft);
      font-size: 0.7rem;
      font-weight: 900;
    }

    .step h3 {
      margin-bottom: 0.5rem;
      font-size: 1.2rem;
    }

    .step p {
      margin-bottom: 0;
      color: var(--muted);
      font-size: 0.84rem;
    }

    .testimonial {
      display: grid;
      grid-template-columns: 0.7fr 1.3fr;
      gap: 4rem;
      align-items: center;
      padding-block: 8rem;
    }

    .quote-orbit {
      position: relative;
      display: grid;
      min-height: 320px;
      place-items: center;
    }

    .quote-orbit::before,
    .quote-orbit::after {
      position: absolute;
      border: 1px solid var(--line);
      border-radius: 999px;
      content: "";
    }

    .quote-orbit::before {
      width: 280px;
      height: 280px;
    }

    .quote-orbit::after {
      width: 190px;
      height: 190px;
      border-color: rgba(105, 230, 212, 0.2);
    }

    .quote-avatar {
      z-index: 1;
      display: grid;
      width: 6rem;
      height: 6rem;
      place-items: center;
      border: 1px solid rgba(255, 255, 255, 0.2);
      border-radius: 2rem;
      background: linear-gradient(145deg, var(--violet), var(--cyan));
      box-shadow: 0 20px 60px rgba(155, 135, 245, 0.2);
      color: #0a0c12;
      font-size: 1.5rem;
      font-weight: 950;
      transform: rotate(-7deg);
    }

    blockquote {
      margin: 0;
    }

    blockquote p {
      margin-bottom: 1.6rem;
      font-size: clamp(1.8rem, 3.6vw, 3.2rem);
      line-height: 1.18;
      letter-spacing: -0.045em;
    }

    blockquote footer {
      color: var(--muted);
      font-size: 0.82rem;
    }

    blockquote footer strong {
      display: block;
      color: var(--ink);
      font-size: 0.9rem;
    }

    .cta-panel {
      position: relative;
      display: grid;
      grid-template-columns: 1fr auto;
      align-items: center;
      gap: 2rem;
      margin-bottom: 4rem;
      padding: clamp(2rem, 6vw, 4.5rem);
      overflow: hidden;
      border: 1px solid rgba(255, 255, 255, 0.14);
      border-radius: 1.5rem;
      background:
        radial-gradient(circle at 90% 10%, rgba(105, 230, 212, 0.17), transparent 20rem),
        linear-gradient(135deg, rgba(155, 135, 245, 0.16), rgba(255, 255, 255, 0.025));
    }

    .cta-panel h2 {
      max-width: 700px;
      margin-bottom: 0.7rem;
      font-size: clamp(2rem, 4.5vw, 4rem);
      line-height: 1;
      letter-spacing: -0.055em;
    }

    .cta-panel p {
      max-width: 620px;
      margin-bottom: 0;
      color: var(--muted);
    }

    .site-footer {
      display: flex;
      align-items: center;
      justify-content: space-between;
      gap: 1rem;
      min-height: 90px;
      border-top: 1px solid var(--line);
      color: var(--muted);
      font-size: 0.75rem;
    }

    .footer-links {
      display: flex;
      gap: 1rem;
    }

    .footer-links a:hover {
      color: var(--ink);
    }

    @media (max-width: 980px) {
      .nav {
        display: none;
      }

      .hero {
        grid-template-columns: 1fr;
        min-height: auto;
      }

      .dashboard-wrap {
        width: min(680px, 100%);
      }

      .dashboard {
        transform: none;
      }

      .feature-card,
      .feature-card.card-wide {
        grid-column: span 6;
      }

      .testimonial {
        grid-template-columns: 0.8fr 1.2fr;
        gap: 2rem;
      }
    }

    @media (max-width: 700px) {
      .header-link,
      .brand-copy small {
        display: none;
      }

      .hero {
        padding-block: 4rem;
      }

      h1 {
        font-size: clamp(3rem, 16vw, 5.2rem);
      }

      .announcement {
        align-items: flex-start;
        border-radius: 0.8rem;
      }

      .dashboard-body {
        grid-template-columns: 1fr;
      }

      .side-rail {
        display: none;
      }

      .metric-grid,
      .step-grid {
        grid-template-columns: 1fr;
      }

      .chart {
        height: 90px;
      }

      .stat-band {
        grid-template-columns: repeat(2, 1fr);
      }

      .stat:nth-child(2) {
        border-right: 0;
      }

      .stat:nth-child(-n + 2) {
        border-bottom: 1px solid var(--line);
      }

      .section-heading,
      .testimonial,
      .cta-panel {
        grid-template-columns: 1fr;
      }

      .feature-card,
      .feature-card.card-wide {
        grid-column: 1 / -1;
      }

      .step-number {
        margin-bottom: 2rem;
      }

      .quote-orbit {
        min-height: 240px;
      }

      .quote-orbit::before {
        width: 220px;
        height: 220px;
      }

      .quote-orbit::after {
        width: 150px;
        height: 150px;
      }

      .site-footer {
        align-items: flex-start;
        flex-direction: column;
        justify-content: center;
        padding-block: 1.5rem;
      }
    }

    @media (prefers-reduced-motion: reduce) {
      html {
        scroll-behavior: auto;
      }

      *,
      *::before,
      *::after {
        transition-duration: 0.01ms !important;
      }
    }
  </style>
</head>
<body>
  <div class="ambient ambient-one" aria-hidden="true"></div>
  <div class="ambient ambient-two" aria-hidden="true"></div>

  <header class="site-header shell">
    <a class="brand" href="/" aria-label="{{brand_name}} home">
      <span class="brand-mark" aria-hidden="true">N</span>
      <span class="brand-copy">
        <strong>{{brand_name}}</strong>
        <small>{{brand_tagline}}</small>
      </span>
    </a>

    <nav class="nav" aria-label="Primary navigation">
      {{#nav_items}}
      <a class="{{state}}" href="{{href}}"{{#current}} aria-current="page"{{/current}}>{{label}}</a>
      {{/nav_items}}
    </nav>

    <a class="header-link" href="#start">{{header_cta}} &rarr;</a>
  </header>

  <main>
    <section class="hero shell" id="overview">
      <div class="hero-copy">
        {{#announcement}}
        <div class="announcement">
          <span class="announcement-badge">{{badge}}</span>
          <span>{{message}} <a href="{{href}}">{{link_label}} &rarr;</a></span>
        </div>
        {{/announcement}}

        <p class="eyebrow">{{eyebrow}}</p>
        <h1>{{hero_title}} <span>{{hero_highlight}}</span></h1>
        <p>{{hero_description}}</p>

        <div class="hero-actions">
          <a class="button button-primary" href="#capabilities">{{primary_cta}}</a>
          <a class="button button-secondary" href="#workflow">{{secondary_cta}} &darr;</a>
        </div>

        <div class="trust-row">
          <span class="avatar-stack" aria-hidden="true">
            {{#team}}
            <span>{{initials}}</span>
            {{/team}}
          </span>
          <span>{{trust_line}}</span>
        </div>
      </div>

      <div class="dashboard-wrap" aria-label="Live service dashboard preview">
        <div class="dashboard">
          <div class="dashboard-bar">
            <span class="window-dots" aria-hidden="true"><i></i><i></i><i></i></span>
            <span class="live-status">{{dashboard_status}}</span>
          </div>

          <div class="dashboard-body">
            <aside class="side-rail" aria-hidden="true">
              <span></span><span></span><span></span><span></span><span></span><span></span>
            </aside>

            <div class="dashboard-main">
              <div class="dashboard-heading">
                <span>
                  <small>{{dashboard_eyebrow}}</small>
                  <strong>{{dashboard_title}}</strong>
                </span>
                <code>{{environment}}</code>
              </div>

              <div class="metric-grid">
                {{#metrics}}
                <article class="metric">
                  <small>{{label}}</small>
                  <strong>{{value}}</strong>
                  <em>{{change}}</em>
                </article>
                {{/metrics}}
              </div>

              <div class="chart" role="img" aria-label="Request volume trending upward"></div>

              <div class="activity-list">
                {{#activities}}
                <article class="activity">
                  <span class="activity-marker {{tone}}">{{marker}}</span>
                  <span class="activity-copy">
                    <strong>{{title}}</strong>
                    <small>{{description}}</small>
                  </span>
                  <time>{{time}}</time>
                </article>
                {{/activities}}
              </div>
            </div>
          </div>
        </div>
      </div>
    </section>

    <section class="stat-band shell" aria-label="Key platform metrics">
      {{#stats}}
      <article class="stat">
        <strong>{{value}}</strong>
        <span>{{label}}</span>
        <small>{{detail}}</small>
      </article>
      {{/stats}}
    </section>

    <section class="features shell" id="capabilities">
      <div class="section-heading">
        <div>
          <p class="eyebrow">{{features_eyebrow}}</p>
          <h2>{{features_title}}</h2>
        </div>
        <p>{{features_description}}</p>
      </div>

      <div class="feature-grid">
        {{#features}}
        <article class="feature-card {{layout}}">
          <div class="feature-topline">
            <span class="feature-icon" aria-hidden="true">{{icon}}</span>
            {{#is_new}}<span class="feature-badge">New</span>{{/is_new}}
          </div>
          <p class="kicker">{{kicker}}</p>
          <h3>{{title}}</h3>
          <p>{{description}}</p>
        </article>
        {{/features}}
      </div>
    </section>

    <section class="workflow" id="workflow">
      <div class="shell">
        <div class="section-heading">
          <div>
            <p class="eyebrow">{{workflow_eyebrow}}</p>
            <h2>{{workflow_title}}</h2>
          </div>
          <p>{{workflow_description}}</p>
        </div>

        <div class="step-grid">
          {{#steps}}
          <article class="step">
            <span class="step-number">{{number}}</span>
            <h3>{{title}}</h3>
            <p>{{description}}</p>
          </article>
          {{/steps}}
        </div>
      </div>
    </section>

    {{#testimonial}}
    <section class="testimonial shell">
      <div class="quote-orbit" aria-hidden="true">
        <span class="quote-avatar">{{initials}}</span>
      </div>
      <blockquote>
        <p>&ldquo;{{quote}}&rdquo;</p>
        <footer>
          <strong>{{name}}</strong>
          {{role}}
        </footer>
      </blockquote>
    </section>
    {{/testimonial}}

    <section class="cta-panel shell" id="start">
      <div>
        <p class="eyebrow">{{cta_eyebrow}}</p>
        <h2>{{cta_title}}</h2>
        <p>{{cta_description}}</p>
      </div>
      <a class="button button-primary" href="/healthcheck">{{cta_button}} &rarr;</a>
    </section>
  </main>

  <footer class="site-footer shell">
    <span>&copy; {{current_year}} {{brand_name}}. {{footer_note}}</span>
    <nav class="footer-links" aria-label="Footer navigation">
      {{#footer_links}}
      <a href="{{href}}">{{label}}</a>
      {{/footer_links}}
    </nav>
  </footer>
</body>
</html>
)mustache";

}  // namespace noroi::templates

#endif  // NOROI_TEMPLATE_HPP_
