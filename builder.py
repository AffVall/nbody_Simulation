#!/usr/bin/env python3
"""
N-Body Builder — Visual preset editor for the n-body simulator.
Configure bodies, physics, and see them on a draggable grid.
"""

import sys
import os
import json
import math
from pathlib import Path

from PyQt5.QtWidgets import (
    QApplication, QMainWindow, QWidget, QVBoxLayout, QHBoxLayout,
    QSplitter, QTableWidget, QTableWidgetItem, QHeaderView,
    QPushButton, QLabel, QLineEdit, QComboBox, QCheckBox,
    QGroupBox, QFormLayout, QColorDialog, QFileDialog,
    QDoubleSpinBox, QSpinBox, QMessageBox, QToolBar, QAction,
    QScrollArea, QFrame, QSizePolicy
)
from PyQt5.QtCore import Qt, QPoint, pyqtSignal
from PyQt5.QtGui import QColor, QFont, QIcon

import matplotlib
matplotlib.use('Qt5Agg')
from matplotlib.backends.backend_qt5agg import FigureCanvasQTAgg as FigureCanvas
from matplotlib.figure import Figure
import matplotlib.pyplot as plt
import numpy as np


PRESETS_DIR = Path(__file__).parent / "presets"

BODY_TYPES = ["star", "planet", "moon", "comet", "asteroid"]
INTEGRATORS = ["yoshida4", "rk4", "ias15", "ahmad_cohen"]
FORCE_METHODS = ["direct", "barnes_hut"]
COLLISION_MODES = ["none", "merge", "elastic"]

DEFAULT_PRESET = {
    "name": "New Preset",
    "gravity": 39.478,
    "time_step": 0.001,
    "integrator": "yoshida4",
    "relativistic": False,
    "pn_order": 0,
    "speed_of_light": 63241.1,
    "collision": "none",
    "force_method": "direct",
    "barnes_hut_theta": 0.5,
    "tolerance": 1e-10,
    "bodies": []
}

DEFAULT_BODY = {
    "name": "Body",
    "type": "planet",
    "mass": 1.0,
    "radius": 0.01,
    "position": [0.0, 0.0, 0.0],
    "velocity": [0.0, 0.0, 0.0],
    "color": [1.0, 1.0, 1.0]
}


class GridCanvas(FigureCanvas):
    """Matplotlib canvas with draggable bodies and auto-resizing grid."""

    body_moved = pyqtSignal(int, float, float)
    body_selected = pyqtSignal(int)

    def __init__(self, parent=None):
        self.fig = Figure(figsize=(6, 6), dpi=100, facecolor='#1a1a2e')
        self.ax = self.fig.add_subplot(111)
        super().__init__(self.fig)
        self.setParent(parent)

        self.ax.set_facecolor('#1a1a2e')
        self.ax.tick_params(colors='#888888', labelsize=8)
        self.ax.minorticks_off()
        self.ax.grid(False, which='minor')
        self.ax.spines['bottom'].set_color('#444444')
        self.ax.spines['top'].set_color('#444444')
        self.ax.spines['left'].set_color('#444444')
        self.ax.spines['right'].set_color('#444444')

        self.setMinimumSize(200, 200)
        self.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Expanding)

        self.bodies = []
        self.selected_idx = -1
        self.dragging_idx = -1
        self.dragging = False
        self._dirty = False

        self._patches = []
        self._glow_patches = []
        self._annotations = []
        self._select_ring = None
        self._grid_set = False

        self.grid_margin = 1.5

        self.fig.canvas.mpl_connect('button_press_event', self.on_press)
        self.fig.canvas.mpl_connect('button_release_event', self.on_release)
        self.fig.canvas.mpl_connect('motion_notify_event', self.on_motion)

    def set_bodies(self, bodies):
        self.bodies = bodies
        self.selected_idx = -1
        self.dragging_idx = -1
        self.dragging = False
        self._full_redraw()

    def _clear_artists(self):
        for p in self._patches:
            p.remove()
        for g in self._glow_patches:
            g.remove()
        for a in self._annotations:
            a.remove()
        if self._select_ring is not None:
            self._select_ring.remove()
            self._select_ring = None
        self._patches = []
        self._glow_patches = []
        self._annotations = []

    def _full_redraw(self):
        self._clear_artists()

        if not self.bodies:
            self.ax.set_xlim(-2, 2)
            self.ax.set_ylim(-2, 2)
            self._apply_grid()
            self.draw_idle()
            return

        xs = [b["position"][0] for b in self.bodies]
        ys = [b["position"][1] for b in self.bodies]
        xmin, xmax = min(xs), max(xs)
        ymin, ymax = min(ys), max(ys)
        span = max(xmax - xmin, ymax - ymin, 0.5) * self.grid_margin
        cx = (xmax + xmin) / 2
        cy = (ymax + ymin) / 2
        self.ax.set_xlim(cx - span, cx + span)
        self.ax.set_ylim(cy - span, cy + span)

        for i, b in enumerate(self.bodies):
            x, y = b["position"][0], b["position"][1]
            r = b.get("radius", 0.01)
            visual_r = max(0.01, min(span * 0.05, r * 200))
            color = b.get("color", [1, 1, 1])
            c = (color[0], color[1], color[2])

            glow = plt.Circle((x, y), visual_r * 1.5, color=c, alpha=0.15, zorder=4)
            self.ax.add_patch(glow)
            self._glow_patches.append(glow)

            circle = plt.Circle((x, y), visual_r, color=c, alpha=0.9, zorder=5)
            self.ax.add_patch(circle)
            self._patches.append(circle)

            ann = self.ax.annotate(
                b["name"], (x, y), textcoords="offset points",
                xytext=(0, min(visual_r * 100, 40) + 8), ha='center',
                fontsize=7, color='#cccccc', zorder=6
            )
            self._annotations.append(ann)

        self._apply_grid()
        self.draw_idle()

    def _apply_grid(self):
        self.ax.minorticks_off()
        self.ax.grid(True, which='major', color='#333355', linewidth=0.5, alpha=0.5)
        self.ax.grid(False, which='minor')
        self.ax.set_aspect('equal')
        self.ax.set_xlabel('x (AU)', color='#888888', fontsize=8)
        self.ax.set_ylabel('y (AU)', color='#888888', fontsize=8)

    def _update_positions(self):
        if not self.bodies:
            return
        xs = [b["position"][0] for b in self.bodies]
        ys = [b["position"][1] for b in self.bodies]
        xmin, xmax = min(xs), max(xs)
        ymin, ymax = min(ys), max(ys)
        span = max(xmax - xmin, ymax - ymin, 0.5) * self.grid_margin
        cx = (xmax + xmin) / 2
        cy = (ymax + ymin) / 2
        self.ax.set_xlim(cx - span, cx + span)
        self.ax.set_ylim(cy - span, cy + span)

        for i, b in enumerate(self.bodies):
            if i >= len(self._patches):
                break
            x, y = b["position"][0], b["position"][1]
            r = b.get("radius", 0.01)
            visual_r = max(0.01, min(span * 0.05, r * 200))

            self._patches[i].center = (x, y)
            self._patches[i].set_radius(visual_r)
            self._glow_patches[i].center = (x, y)
            self._glow_patches[i].set_radius(visual_r * 1.5)
            self._annotations[i].xy = (x, y)
            self._annotations[i].xyann = (0, min(visual_r * 100, 40) + 8)

        self._update_select_ring(span)
        self.draw_idle()

    def _update_select_ring(self, span=None):
        if self.selected_idx < 0 or self.selected_idx >= len(self.bodies):
            if self._select_ring is not None:
                self._select_ring.remove()
                self._select_ring = None
            return
        if span is None:
            xs = [b["position"][0] for b in self.bodies]
            ys = [b["position"][1] for b in self.bodies]
            xmin, xmax = min(xs), max(xs)
            ymin, ymax = min(ys), max(ys)
            span = max(xmax - xmin, ymax - ymin, 0.5) * self.grid_margin
        b = self.bodies[self.selected_idx]
        x, y = b["position"][0], b["position"][1]
        r = b.get("radius", 0.01)
        visual_r = max(0.01, min(span * 0.05, r * 200))
        if self._select_ring is not None:
            self._select_ring.remove()
        self._select_ring = plt.Circle(
            (x, y), visual_r * 2.0, fill=False,
            edgecolor='#ffffff', linewidth=1.5, linestyle='--', alpha=0.7, zorder=7
        )
        self.ax.add_patch(self._select_ring)

    def _find_body_at(self, event):
        if not self.bodies or event.xdata is None:
            return -1
        xmin, xmax = self.ax.get_xlim()
        ymin, ymax = self.ax.get_ylim()
        scale = max(xmax - xmin, ymax - ymin)
        threshold = scale * 0.03

        for i, b in enumerate(self.bodies):
            dx = event.xdata - b["position"][0]
            dy = event.ydata - b["position"][1]
            if math.sqrt(dx * dx + dy * dy) < threshold:
                return i
        return -1

    def on_press(self, event):
        if event.inaxes != self.ax:
            return
        idx = self._find_body_at(event)
        if idx >= 0:
            self.selected_idx = idx
            self.dragging_idx = idx
            self.dragging = True
            self._update_positions()
            self.body_selected.emit(idx)
            self.fig.canvas.set_cursor(Qt.ClosedHandCursor)
        else:
            self.selected_idx = -1
            self.dragging_idx = -1
            self.dragging = False
            self._update_positions()
            self.fig.canvas.set_cursor(Qt.ArrowCursor)

    def on_release(self, event):
        self.dragging = False
        self.dragging_idx = -1
        self.fig.canvas.set_cursor(Qt.ArrowCursor)

    def on_motion(self, event):
        if not self.dragging or self.dragging_idx < 0:
            return
        if event.xdata is None or event.ydata is None:
            return
        idx = self.dragging_idx
        self.bodies[idx]["position"][0] = round(event.xdata, 6)
        self.bodies[idx]["position"][1] = round(event.ydata, 6)
        self.body_moved.emit(idx, event.xdata, event.ydata)
        self._update_positions()

    def resizeEvent(self, event):
        super().resizeEvent(event)
        self.fig.set_size_inches(
            self.width() / self.fig.dpi,
            self.height() / self.fig.dpi
        )
        self.draw_idle()


class BodyEditorWidget(QWidget):
    """Right-side panel for editing a single body's properties."""

    changed = pyqtSignal()

    def __init__(self, parent=None):
        super().__init__(parent)
        self.body = None
        self._updating = False
        self._init_ui()

    def _init_ui(self):
        layout = QFormLayout(self)
        layout.setContentsMargins(8, 8, 8, 8)
        layout.setSpacing(4)

        self.name_edit = QLineEdit()
        self.name_edit.textChanged.connect(self._on_change)
        layout.addRow("Nome:", self.name_edit)

        self.type_combo = QComboBox()
        self.type_combo.addItems(BODY_TYPES)
        self.type_combo.currentTextChanged.connect(self._on_change)
        layout.addRow("Tipo:", self.type_combo)

        self.mass_edit = QDoubleSpinBox()
        self.mass_edit.setRange(1e-30, 1e30)
        self.mass_edit.setDecimals(10)
        self.mass_edit.setSingleStep(0.001)
        self.mass_edit.valueChanged.connect(self._on_change)
        layout.addRow("Massa (M☉):", self.mass_edit)

        self.radius_edit = QDoubleSpinBox()
        self.radius_edit.setRange(1e-30, 100)
        self.radius_edit.setDecimals(10)
        self.radius_edit.setSingleStep(0.001)
        self.radius_edit.valueChanged.connect(self._on_change)
        layout.addRow("Raio (AU):", self.radius_edit)

        pos_group = QGroupBox("Posição (AU)")
        pos_lay = QFormLayout(pos_group)
        self.posx_edit = QDoubleSpinBox()
        self.posx_edit.setRange(-1000, 1000)
        self.posx_edit.setDecimals(6)
        self.posx_edit.valueChanged.connect(self._on_change)
        pos_lay.addRow("X:", self.posx_edit)
        self.posy_edit = QDoubleSpinBox()
        self.posy_edit.setRange(-1000, 1000)
        self.posy_edit.setDecimals(6)
        self.posy_edit.valueChanged.connect(self._on_change)
        pos_lay.addRow("Y:", self.posy_edit)
        self.posz_edit = QDoubleSpinBox()
        self.posz_edit.setRange(-1000, 1000)
        self.posz_edit.setDecimals(6)
        self.posz_edit.valueChanged.connect(self._on_change)
        pos_lay.addRow("Z:", self.posz_edit)
        layout.addRow(pos_group)

        vel_group = QGroupBox("Velocidade (AU/yr)")
        vel_lay = QFormLayout(vel_group)
        self.velx_edit = QDoubleSpinBox()
        self.velx_edit.setRange(-10000, 10000)
        self.velx_edit.setDecimals(4)
        self.velx_edit.valueChanged.connect(self._on_change)
        vel_lay.addRow("Vx:", self.velx_edit)
        self.vely_edit = QDoubleSpinBox()
        self.vely_edit.setRange(-10000, 10000)
        self.vely_edit.setDecimals(4)
        self.vely_edit.valueChanged.connect(self._on_change)
        vel_lay.addRow("Vy:", self.vely_edit)
        self.velz_edit = QDoubleSpinBox()
        self.velz_edit.setRange(-10000, 10000)
        self.velz_edit.setDecimals(4)
        self.velz_edit.valueChanged.connect(self._on_change)
        vel_lay.addRow("Vz:", self.velz_edit)
        layout.addRow(vel_group)

        self.color_btn = QPushButton("  Cor  ")
        self.color_btn.clicked.connect(self._pick_color)
        layout.addRow("Cor:", self.color_btn)

    def load_body(self, body):
        self.body = body
        self._updating = True
        self.name_edit.setText(body.get("name", ""))
        idx = self.type_combo.findText(body.get("type", "planet"))
        if idx >= 0:
            self.type_combo.setCurrentIndex(idx)
        self.mass_edit.setValue(body.get("mass", 1.0))
        self.radius_edit.setValue(body.get("radius", 0.01))
        pos = body.get("position", [0, 0, 0])
        self.posx_edit.setValue(pos[0])
        self.posy_edit.setValue(pos[1])
        self.posz_edit.setValue(pos[2])
        vel = body.get("velocity", [0, 0, 0])
        self.velx_edit.setValue(vel[0])
        self.vely_edit.setValue(vel[1])
        self.velz_edit.setValue(vel[2])
        color = body.get("color", [1, 1, 1])
        self.color_btn.setStyleSheet(
            f"background-color: rgb({int(color[0]*255)},{int(color[1]*255)},{int(color[2]*255)})")
        self._updating = False

    def _on_change(self):
        if self._updating or self.body is None:
            return
        self.body["name"] = self.name_edit.text()
        self.body["type"] = self.type_combo.currentText()
        self.body["mass"] = self.mass_edit.value()
        self.body["radius"] = self.radius_edit.value()
        self.body["position"][0] = self.posx_edit.value()
        self.body["position"][1] = self.posy_edit.value()
        self.body["position"][2] = self.posz_edit.value()
        self.body["velocity"][0] = self.velx_edit.value()
        self.body["velocity"][1] = self.vely_edit.value()
        self.body["velocity"][2] = self.velz_edit.value()
        self.changed.emit()

    def update_position_only(self, x, y):
        if self.body is None or self._updating:
            return
        self._updating = True
        self.posx_edit.blockSignals(True)
        self.posy_edit.blockSignals(True)
        self.posx_edit.setValue(x)
        self.posy_edit.setValue(y)
        self.body["position"][0] = x
        self.body["position"][1] = y
        self.posx_edit.blockSignals(False)
        self.posy_edit.blockSignals(False)
        self._updating = False

    def _pick_color(self):
        if self.body is None:
            return
        color = self.body.get("color", [1, 1, 1])
        c = QColor.fromRgbF(color[0], color[1], color[2])
        new_color = QColorDialog.getColor(c, self, "Cor do corpo")
        if new_color.isValid():
            self.body["color"] = [new_color.redF(), new_color.greenF(), new_color.blueF()]
            self.color_btn.setStyleSheet(
                f"background-color: {new_color.name()}")
            self.changed.emit()


class BuilderWindow(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("N-Body Builder")
        self.setMinimumSize(1200, 700)

        self.preset = json.loads(json.dumps(DEFAULT_PRESET))
        self.preset["bodies"] = []
        self.selected_body_idx = -1
        self.current_file = None

        self._init_ui()
        self._load_preset_names()
        self._add_default_bodies()

    def _init_ui(self):
        central = QWidget()
        self.setCentralWidget(central)
        main_layout = QHBoxLayout(central)
        main_layout.setContentsMargins(4, 4, 4, 4)

        splitter = QSplitter(Qt.Horizontal)
        main_layout.addWidget(splitter)

        left_widget = QWidget()
        left_layout = QVBoxLayout(left_widget)
        left_layout.setContentsMargins(4, 4, 4, 4)

        toolbar = QToolBar()
        toolbar.setMovable(False)
        self.addToolBar(toolbar)

        load_action = QAction("Carregar", self)
        load_action.triggered.connect(self._load_file)
        toolbar.addAction(load_action)

        save_action = QAction("Salvar", self)
        save_action.triggered.connect(self._save_file)
        toolbar.addAction(save_action)

        save_as_action = QAction("Salvar Como", self)
        save_as_action.triggered.connect(self._save_file_as)
        toolbar.addAction(save_as_action)

        toolbar.addSeparator()

        add_body_action = QAction("+ Corpo", self)
        add_body_action.triggered.connect(self._add_body)
        toolbar.addAction(add_body_action)

        remove_body_action = QAction("- Corpo", self)
        remove_body_action.triggered.connect(self._remove_body)
        toolbar.addAction(remove_body_action)

        toolbar.addSeparator()

        run_action = QAction("▶ Rodar Simulação", self)
        run_action.triggered.connect(self._run_simulation)
        toolbar.addAction(run_action)

        preset_label = QLabel("  Preset: ")
        toolbar.addWidget(preset_label)
        self.preset_combo = QComboBox()
        self.preset_combo.setMinimumWidth(180)
        self.preset_combo.currentTextChanged.connect(self._on_preset_selected)
        toolbar.addWidget(self.preset_combo)

        scroll = QScrollArea()
        scroll.setWidgetResizable(True)
        scroll_content = QWidget()
        scroll_layout = QVBoxLayout(scroll_content)
        scroll_layout.setContentsMargins(4, 4, 4, 4)

        sim_group = QGroupBox("Simulação")
        sim_form = QFormLayout(sim_group)
        self.name_edit = QLineEdit()
        self.name_edit.textChanged.connect(self._on_sim_change)
        sim_form.addRow("Nome:", self.name_edit)
        self.gravity_edit = QDoubleSpinBox()
        self.gravity_edit.setRange(0.001, 10000)
        self.gravity_edit.setDecimals(4)
        self.gravity_edit.setSingleStep(0.01)
        self.gravity_edit.setValue(39.478)
        self.gravity_edit.valueChanged.connect(self._on_sim_change)
        sim_form.addRow("Gravidade (G):", self.gravity_edit)
        self.dt_edit = QDoubleSpinBox()
        self.dt_edit.setRange(0.000001, 10)
        self.dt_edit.setDecimals(7)
        self.dt_edit.setSingleStep(0.0001)
        self.dt_edit.setValue(0.001)
        self.dt_edit.valueChanged.connect(self._on_sim_change)
        sim_form.addRow("Passo (dt):", self.dt_edit)
        scroll_layout.addWidget(sim_group)

        integrator_group = QGroupBox("Integrador")
        int_form = QFormLayout(integrator_group)
        self.integrator_combo = QComboBox()
        self.integrator_combo.addItems(INTEGRATORS)
        self.integrator_combo.currentTextChanged.connect(self._on_sim_change)
        int_form.addRow("Método:", self.integrator_combo)
        self.tolerance_edit = QDoubleSpinBox()
        self.tolerance_edit.setRange(1e-20, 1)
        self.tolerance_edit.setDecimals(10)
        self.tolerance_edit.setSingleStep(1e-10)
        self.tolerance_edit.setValue(1e-10)
        self.tolerance_edit.valueChanged.connect(self._on_sim_change)
        int_form.addRow("Tolerância:", self.tolerance_edit)
        scroll_layout.addWidget(integrator_group)

        physics_group = QGroupBox("Física")
        phys_form = QFormLayout(physics_group)
        self.force_combo = QComboBox()
        self.force_combo.addItems(FORCE_METHODS)
        self.force_combo.currentTextChanged.connect(self._on_sim_change)
        phys_form.addRow("Força:", self.force_combo)
        self.collision_combo = QComboBox()
        self.collision_combo.addItems(COLLISION_MODES)
        self.collision_combo.currentTextChanged.connect(self._on_sim_change)
        phys_form.addRow("Colisão:", self.collision_combo)
        self.relativistic_check = QCheckBox("Correção Relativística")
        self.relativistic_check.stateChanged.connect(self._on_sim_change)
        phys_form.addRow(self.relativistic_check)
        self.pn_order_spin = QSpinBox()
        self.pn_order_spin.setRange(0, 3)
        self.pn_order_spin.setValue(0)
        self.pn_order_spin.valueChanged.connect(self._on_sim_change)
        phys_form.addRow("Ordem PN:", self.pn_order_spin)
        self.bh_theta_edit = QDoubleSpinBox()
        self.bh_theta_edit.setRange(0.01, 5.0)
        self.bh_theta_edit.setDecimals(2)
        self.bh_theta_edit.setValue(0.5)
        self.bh_theta_edit.valueChanged.connect(self._on_sim_change)
        phys_form.addRow("θ Barnes-Hut:", self.bh_theta_edit)
        scroll_layout.addWidget(physics_group)

        bodies_group = QGroupBox("Corpos")
        bodies_layout = QVBoxLayout(bodies_group)
        self.bodies_table = QTableWidget()
        self.bodies_table.setColumnCount(4)
        self.bodies_table.setHorizontalHeaderLabels(["Nome", "Massa", "Posição", "Tipo"])
        self.bodies_table.horizontalHeader().setSectionResizeMode(QHeaderView.Stretch)
        self.bodies_table.setSelectionBehavior(QTableWidget.SelectRows)
        self.bodies_table.setSelectionMode(QTableWidget.SingleSelection)
        self.bodies_table.currentCellChanged.connect(self._on_body_selected)
        bodies_layout.addWidget(self.bodies_table)

        body_btns = QHBoxLayout()
        btn_add = QPushButton("+ Adicionar")
        btn_add.clicked.connect(self._add_body)
        btn_remove = QPushButton("- Remover")
        btn_remove.clicked.connect(self._remove_body)
        body_btns.addWidget(btn_add)
        body_btns.addWidget(btn_remove)
        bodies_layout.addLayout(body_btns)
        scroll_layout.addWidget(bodies_group)

        scroll_layout.addStretch()
        scroll.setWidget(scroll_content)
        left_layout.addWidget(scroll)

        right_widget = QWidget()
        right_layout = QVBoxLayout(right_widget)
        right_layout.setContentsMargins(4, 4, 4, 4)

        self.grid_canvas = GridCanvas(self)
        self.grid_canvas.body_moved.connect(self._on_body_dragged)
        self.grid_canvas.body_selected.connect(self._on_body_selected_from_grid)
        right_layout.addWidget(self.grid_canvas)

        self.body_editor = BodyEditorWidget(self)
        self.body_editor.changed.connect(self._on_body_editor_changed)
        right_layout.addWidget(self.body_editor)

        splitter.addWidget(left_widget)
        splitter.addWidget(right_widget)
        splitter.setSizes([400, 800])

    def _load_preset_names(self):
        self.preset_combo.blockSignals(True)
        self.preset_combo.clear()
        self.preset_combo.addItem("< Novo >")
        if PRESETS_DIR.exists():
            for f in sorted(PRESETS_DIR.glob("*.json")):
                self.preset_combo.addItem(f.stem)
        self.preset_combo.blockSignals(False)

    def _on_preset_selected(self, name):
        if name == "< Novo >":
            self.preset = json.loads(json.dumps(DEFAULT_PRESET))
            self.preset["bodies"] = []
            self.current_file = None
        else:
            path = PRESETS_DIR / f"{name}.json"
            if path.exists():
                with open(path) as f:
                    self.preset = json.load(f)
                self.current_file = path
        self._refresh_ui()

    def _refresh_ui(self):
        self.name_edit.setText(self.preset.get("name", ""))
        self.gravity_edit.setValue(self.preset.get("gravity", 39.478))
        self.dt_edit.setValue(self.preset.get("time_step", 0.001))
        idx = self.integrator_combo.findText(self.preset.get("integrator", "yoshida4"))
        if idx >= 0:
            self.integrator_combo.setCurrentIndex(idx)
        self.tolerance_edit.setValue(self.preset.get("tolerance", 1e-10))
        idx = self.force_combo.findText(self.preset.get("force_method", "direct"))
        if idx >= 0:
            self.force_combo.setCurrentIndex(idx)
        idx = self.collision_combo.findText(self.preset.get("collision", "none"))
        if idx >= 0:
            self.collision_combo.setCurrentIndex(idx)
        self.relativistic_check.setChecked(self.preset.get("relativistic", False))
        self.pn_order_spin.setValue(self.preset.get("pn_order", 0))
        self.bh_theta_edit.setValue(self.preset.get("barnes_hut_theta", 0.5))
        self._refresh_bodies_table()
        self.grid_canvas.set_bodies(self.preset["bodies"])

    def _refresh_bodies_table(self):
        self.bodies_table.blockSignals(True)
        self.bodies_table.setRowCount(len(self.preset["bodies"]))
        for i, b in enumerate(self.preset["bodies"]):
            self.bodies_table.setItem(i, 0, QTableWidgetItem(b["name"]))
            self.bodies_table.setItem(i, 1, QTableWidgetItem(f'{b["mass"]:.4e}'))
            pos = b["position"]
            self.bodies_table.setItem(i, 2, QTableWidgetItem(f'({pos[0]:.2f}, {pos[1]:.2f})'))
            self.bodies_table.setItem(i, 3, QTableWidgetItem(b.get("type", "?")))
        self.bodies_table.blockSignals(False)

    def _on_body_selected(self, row, col, prev_row, prev_col):
        if 0 <= row < len(self.preset["bodies"]):
            self.selected_body_idx = row
            self.body_editor.load_body(self.preset["bodies"][row])
            self.grid_canvas.selected_idx = row
            self.grid_canvas._update_positions()

    def _on_body_dragged(self, idx, x, y):
        self.preset["bodies"][idx]["position"][0] = round(x, 6)
        self.preset["bodies"][idx]["position"][1] = round(y, 6)
        self.body_editor.update_position_only(x, y)
        self.bodies_table.blockSignals(True)
        self.bodies_table.setItem(idx, 2, QTableWidgetItem(f'({x:.2f}, {y:.2f})'))
        self.bodies_table.blockSignals(False)

    def _on_body_selected_from_grid(self, idx):
        if 0 <= idx < len(self.preset["bodies"]):
            self.selected_body_idx = idx
            self.bodies_table.blockSignals(True)
            self.bodies_table.selectRow(idx)
            self.bodies_table.blockSignals(False)
            self.body_editor.load_body(self.preset["bodies"][idx])

    def _on_body_editor_changed(self):
        if 0 <= self.selected_body_idx < len(self.preset["bodies"]):
            self.bodies_table.blockSignals(True)
            self._refresh_bodies_table()
            self.bodies_table.blockSignals(False)
            self.grid_canvas._update_positions()

    def _on_sim_change(self):
        self.preset["name"] = self.name_edit.text()
        self.preset["gravity"] = self.gravity_edit.value()
        self.preset["time_step"] = self.dt_edit.value()
        self.preset["integrator"] = self.integrator_combo.currentText()
        self.preset["tolerance"] = self.tolerance_edit.value()
        self.preset["force_method"] = self.force_combo.currentText()
        self.preset["collision"] = self.collision_combo.currentText()
        self.preset["relativistic"] = self.relativistic_check.isChecked()
        self.preset["pn_order"] = self.pn_order_spin.value()
        self.preset["barnes_hut_theta"] = self.bh_theta_edit.value()

    def _add_body(self):
        n = len(self.preset["bodies"])
        colors = [
            [1.0, 0.3, 0.3], [0.3, 1.0, 0.3], [0.3, 0.3, 1.0],
            [1.0, 1.0, 0.3], [1.0, 0.3, 1.0], [0.3, 1.0, 1.0],
            [1.0, 0.6, 0.2], [0.6, 0.2, 1.0]
        ]
        body = json.loads(json.dumps(DEFAULT_BODY))
        body["name"] = f"Body {n+1}"
        body["color"] = colors[n % len(colors)]
        angle = n * 2 * math.pi / max(n + 1, 3)
        body["position"] = [round(math.cos(angle), 4), round(math.sin(angle), 4), 0]
        self.preset["bodies"].append(body)
        self._refresh_bodies_table()
        self.grid_canvas.set_bodies(self.preset["bodies"])
        self.bodies_table.selectRow(len(self.preset["bodies"]) - 1)

    def _remove_body(self):
        row = self.bodies_table.currentRow()
        if 0 <= row < len(self.preset["bodies"]):
            self.preset["bodies"].pop(row)
            self.selected_body_idx = -1
            self._refresh_bodies_table()
            self.grid_canvas.set_bodies(self.preset["bodies"])
            if self.preset["bodies"]:
                new_row = min(row, len(self.preset["bodies"]) - 1)
                self.bodies_table.selectRow(new_row)

    def _add_default_bodies(self):
        colors = [[1.0, 0.3, 0.3], [0.3, 1.0, 0.3], [0.3, 0.3, 1.0]]
        for i, (name, pos, vel, col) in enumerate([
            ("Body 1", [0.97, -0.243, 0], [2.929, 2.716, 0], colors[0]),
            ("Body 2", [-0.97, 0.243, 0], [2.929, 2.716, 0], colors[1]),
            ("Body 3", [0, 0, 0], [-5.858, -5.433, 0], colors[2]),
        ]):
            body = json.loads(json.dumps(DEFAULT_BODY))
            body["name"] = name
            body["position"] = pos
            body["velocity"] = vel
            body["color"] = col
            self.preset["bodies"].append(body)
        self._refresh_ui()

    def _load_file(self):
        path, _ = QFileDialog.getOpenFileName(
            self, "Carregar Preset", str(PRESETS_DIR),
            "JSON Files (*.json);;All Files (*)")
        if path:
            with open(path) as f:
                self.preset = json.load(f)
            self.current_file = Path(path)
            self._refresh_ui()
            self.setWindowTitle(f"N-Body Builder — {Path(path).name}")

    def _save_file(self):
        if self.current_file:
            self._save_to(self.current_file)
        else:
            self._save_file_as()

    def _save_file_as(self):
        path, _ = QFileDialog.getSaveFileName(
            self, "Salvar Preset", str(PRESETS_DIR / "preset.json"),
            "JSON Files (*.json)")
        if path:
            self._save_to(Path(path))
            self.current_file = Path(path)
            self._load_preset_names()

    def _save_to(self, path):
        self._on_sim_change()
        with open(path, "w") as f:
            json.dump(self.preset, f, indent=4)
        self.statusBar().showMessage(f"Salvo: {path}", 3000)

    def _run_simulation(self):
        self._on_sim_change()
        tmp_path = PRESETS_DIR / "_builder_tmp.json"
        with open(tmp_path, "w") as f:
            json.dump(self.preset, f, indent=4)

        nbody_bin = Path(__file__).parent / "build" / "nbody"
        if not nbody_bin.exists():
            nbody_bin = Path(__file__).parent / "nbody"
        if not nbody_bin.exists():
            QMessageBox.warning(self, "Erro",
                                "Binário nbody não encontrado.\nCompile primeiro com: cd build && cmake .. && make")
            return

        import subprocess
        try:
            subprocess.Popen(
                [str(nbody_bin), str(tmp_path)],
                cwd=str(nbody_bin.parent)
            )
        except Exception as e:
            QMessageBox.critical(self, "Erro", str(e))


def main():
    app = QApplication(sys.argv)
    app.setStyle("Fusion")

    palette = app.palette()
    palette.setColor(palette.Window, QColor("#2b2b3d"))
    palette.setColor(palette.WindowText, QColor("#dddddd"))
    palette.setColor(palette.Base, QColor("#1e1e2e"))
    palette.setColor(palette.AlternateBase, QColor("#252536"))
    palette.setColor(palette.ToolTipBase, QColor("#333355"))
    palette.setColor(palette.ToolTipText, QColor("#dddddd"))
    palette.setColor(palette.Text, QColor("#dddddd"))
    palette.setColor(palette.Button, QColor("#3a3a5c"))
    palette.setColor(palette.ButtonText, QColor("#dddddd"))
    palette.setColor(palette.Highlight, QColor("#5555aa"))
    palette.setColor(palette.HighlightedText, QColor("#ffffff"))
    app.setPalette(palette)

    font = QFont("monospace", 9)
    app.setFont(font)

    window = BuilderWindow()
    window.show()
    sys.exit(app.exec_())


if __name__ == "__main__":
    main()
