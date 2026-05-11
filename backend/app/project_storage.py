from collections.abc import Iterable

from sqlalchemy import Float, Index, Integer, String, create_engine, select
from sqlalchemy.orm import DeclarativeBase, Mapped, Session, mapped_column, sessionmaker

from .config import settings


class ProjectBase(DeclarativeBase):
    pass


class ProjectTelemetry(ProjectBase):
    __tablename__ = "project"
    __table_args__ = (
        Index("ix_project_device_id_id", "device_id", "id"),
    )

    id: Mapped[int] = mapped_column(primary_key=True, autoincrement=True)
    device_id: Mapped[str] = mapped_column(String(255))
    temperature: Mapped[int] = mapped_column(Integer, default=0)
    humidity: Mapped[int] = mapped_column(Integer, default=0)
    speed: Mapped[int] = mapped_column(Integer, default=0)
    direction: Mapped[str] = mapped_column(String(255), default="")
    uv: Mapped[str] = mapped_column(String(255), default="")
    raindrop: Mapped[str] = mapped_column(String(255), default="")
    pm: Mapped[int] = mapped_column(Integer, default=0)
    air_pressure: Mapped[int] = mapped_column(Integer, default=0)
    altitude: Mapped[int] = mapped_column(Integer, default=0)
    pressure: Mapped[str] = mapped_column(String(255), default="")
    distance: Mapped[int] = mapped_column(Integer, default=0)
    electric: Mapped[int] = mapped_column(Integer, default=0)
    posture: Mapped[str] = mapped_column(String(255), default="")
    complex: Mapped[str] = mapped_column(String(255), default="")
    longitude: Mapped[float] = mapped_column(Float, default=0.0)


project_engine = create_engine(settings.project_database_url, future=True, pool_pre_ping=True)
ProjectSessionLocal = sessionmaker(
    bind=project_engine,
    autoflush=False,
    autocommit=False,
    expire_on_commit=False,
    class_=Session,
)


def ensure_project_schema() -> None:
    ProjectBase.metadata.create_all(project_engine)


def project_database_label() -> str:
    url = settings.project_database_url
    if "@" in url:
        return url.rsplit("@", 1)[-1]
    return url


def load_latest_project_rows(
    session: Session,
    *,
    device_ids: Iterable[str] | None = None,
    device_id: str | None = None,
    limit: int = 200,
) -> list[ProjectTelemetry]:
    stmt = select(ProjectTelemetry)
    if device_id is not None and device_id.strip():
        stmt = stmt.where(ProjectTelemetry.device_id == device_id.strip())
    elif device_ids is not None:
        filtered = [item.strip() for item in device_ids if item and item.strip()]
        if filtered:
            stmt = stmt.where(ProjectTelemetry.device_id.in_(filtered))
    return list(session.scalars(stmt.order_by(ProjectTelemetry.id.desc()).limit(limit)).all())


def load_latest_project_by_device_ids(session: Session, device_ids: Iterable[str]) -> dict[str, ProjectTelemetry]:
    rows = load_latest_project_rows(session, device_ids=device_ids, limit=1000)
    latest: dict[str, ProjectTelemetry] = {}
    for row in rows:
        if row.device_id not in latest:
            latest[row.device_id] = row
    return latest
