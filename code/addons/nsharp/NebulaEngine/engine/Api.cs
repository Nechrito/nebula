using System;
using System.Runtime.InteropServices;
using System.Runtime.CompilerServices;
using System.Collections;
using System.Collections.Generic;
using Microsoft.Xna.Framework;

namespace Nebula
{
    namespace Game
    {
        public enum Events
        {
            OnFrame,
            OnActivate,
            OnDeactivate,
            NumEvents
        }

        public delegate void EventDelegate();

        public struct ComponentData<T>
        {
            public T this[int index]
            { 
                get
                {
                    //return (T)buffer.GetValue(index);
                    return buffer[index];
                }
                set
                {
                    // buffer.SetValue(value, index);
                    buffer[index] = value;
                }
            }

            // TODO: should be a native array, with acces methods via internal calls.
            // private T[] buffer;
            private List<T> buffer;
        }

        public sealed class ComponentManager
        {
            private static readonly ComponentManager instance = new ComponentManager();
            // Explicit static constructor to tell C# compiler
            // not to mark type as beforefieldinit
            static ComponentManager()
            {
                // Initialize eventcallbacks list to a specific size.
                Instance.eventCallbacks = new List<List<EventDelegate>>();
                for (int i = 0; i < (int)Events.NumEvents; i++)
                {
                    Instance.eventCallbacks.Add(new List<EventDelegate>());
                }
            }
            private ComponentManager() {}

            public static ComponentManager Instance
            {
                get
                {
                    return instance;
                }
            }

            public static void RegisterComponent(IComponent component)
            {
                component.SetupEvents();
                Instance.registry.Add(component);
            }

            public static void SetupEventDelegate(Events e, EventDelegate func)
            {
                Instance.eventCallbacks[(int)e].Add(func);
            }

            public static void OnFrame()
            {
                int numCallbacks = Instance.eventCallbacks[(int)Events.OnFrame].Count;
                for (int i = 0; i < numCallbacks; ++i)
                {
                    Instance.eventCallbacks[(int)Events.OnFrame][i]();
                }
            }

            private List<IComponent> registry = new List<IComponent>();
            private List<List<EventDelegate>> eventCallbacks;
        }

        public interface IComponent
        {
            void Register(Game.Entity entity);
            void Deregister(Game.Entity entity);
            void SetupEvents();
        }

        public class Component<DATA> : IComponent
        {
            public void Register(Game.Entity entity)
            {
                
            }

            public void Deregister(Game.Entity entity)
            {

            }

            protected void RegisterEvent(Events e, EventDelegate func)
            {
                events.Add(e);
                Game.ComponentManager.SetupEventDelegate(e, func);
            }

            public virtual void SetupEvents()
            {
                // override in subclass
            }

            public readonly List<Events> events = new List<Events>();

            private int size;
            protected DATA data;
            private Dictionary<Entity, InstanceId> entityMap;
        }

        /*
         * Entity   
         */
        public struct Entity : IEquatable<Entity>
        {
            private UInt32 id;
            
            public Entity(uint id)
            {
                this.id = id;
            }

            public uint Id
            {
                get
                {
                    return id;
                }
            }

            /// <summary>
            /// This entitys transform
            /// </summary>
            public Matrix Transform
            {
                get
                {
                    return GetTransform();
                }

                set
                {
                    // TODO: Send set transform message.
                    // maybe check if this entity is registered first and register it if necessary?
                    SetTransform(value);
                }
            }

            /// <summary>
            /// Check whether this entity is valid (alive)
            /// </summary>
            [MethodImplAttribute(MethodImplOptions.InternalCall)]
            public static extern bool IsValid();

            /// <summary>
            /// Convert entity to string representation
            /// </summary>
            public override string ToString() { return this.id.ToString(); }

            /// <summary>
            /// Check if two entities are the same
            /// </summary>
            public bool Equals(Entity other) { return this.id == other.id; }

            /// <summary>
            /// Retrieve the transform of an entity if it is registered to the component
            /// </summary>
            [MethodImplAttribute(MethodImplOptions.InternalCall)]
            private extern Matrix GetTransform();

            [MethodImplAttribute(MethodImplOptions.InternalCall)]
            private extern void SetTransform(Matrix mat);

            
        }

        public struct InstanceId
        {
            private uint id;

            public InstanceId(uint id)
            {
                this.id = id;
            }

            public uint Id
            {
                get
                {
                    return id;
                }
            }
        }
    }

    public class EntityManager
    {
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern Game.Entity CreateEntity();

        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void DeleteEntity(Game.Entity entity);
    }

    public class Debug
    {
        [DllImport ("__Internal", EntryPoint="N_Print")]
        public static extern void Log(string val);
    }

// [DllImport ("__Internal", EntryPoint="Foobar", CharSet=CharSet.Ansi)]
// static extern void Foobar(
//     [MarshalAs (UnmanagedType.CustomMarshaler,
//         MarshalTypeRef=typeof(StringMarshaler))]
//     String val
// );

}

