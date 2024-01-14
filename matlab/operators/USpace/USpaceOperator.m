
classdef USpaceOperator < handle      

properties
    name    string            % Operator name
    models_path               

    UAVs = struct([])         % list to the fleet of UAVs

    % ROS2 interface
    rosNode                   % ROS2 Node 
    rosSub_Time               % ROS2 subscriptor to get simulation time
    rosCli_DeployUAV          % ROS2 Service client to deploy models into the air space
    rosCli_RemoveUAV          % ROS2 Service client to remove models from the air space

end

methods


function obj = USpaceOperator(name,models_path)

    obj.name = name;
    obj.models_path = models_path;

    % ROS2 node
    obj.rosNode = ros2node(obj.name);

    obj.rosSub_Time = ros2subscriber(obj.rosNode, ...
        '/World/Time','builtin_interfaces/Time', ...
        'History','keeplast');

    obj.rosCli_DeployUAV = ros2svcclient(obj.rosNode, ...
        '/World/DeployModel','navsim_msgs/DeployModel', ...
        'History','keepall');

    obj.rosCli_RemoveUAV = ros2svcclient(obj.rosNode, ...
        '/World/RemoveModel','navsim_msgs/RemoveModel', ...
        'History','keepall');

end


function [sec,mil] = GetTime(obj)
    [msg,status,~] = receive(obj.rosSub_Time,1);
    if (status)
        sec = double(msg.sec);
        mil = double(msg.nanosec / 1E6);
    else
        sec = 0;
        mil = 0;
    end
end


function status = DeployUAV(obj,model,UAVid,pos,rot)

    status = false;

    if obj.getUAVindex(UAVid) ~= -1
        return
    end

    uav.id = UAVid;
    uav.model = model;

    switch model
        case UAVmodels.MiniDroneCommanded
            file = fullfile(obj.models_path,'/UAM/minidrone/model.sdf');
            uav.rosPub = ros2publisher(obj.rosNode, ...
                ['/UAV/',UAVid,'/RemoteCommand'],      ...
                "navsim_msgs/RemoteCommand");

        case UAVmodels.MiniDroneFP1
            file = fullfile(obj.models_path,'/UAM/minidrone/model_FP1.sdf');
            uav.rosPub = ros2publisher(obj.rosNode, ...
                ['/UAV/',UAVid,'/FlightPlan'],      ...
                "navsim_msgs/FlightPlan");
        otherwise
            return
    end

    obj.UAVs = [obj.UAVs uav];

    % Call ROS2 service
    req = ros2message(obj.rosCli_DeployUAV);
    req.model_sdf = fileread(file);
    req.name  = UAVid; 
    req.pos.x = pos(1);
    req.pos.y = pos(2);
    req.pos.z = pos(3);
    req.rot.x = rot(1);
    req.rot.y = rot(2);
    req.rot.z = rot(3);

    status = waitForServer(obj.rosCli_DeployUAV,"Timeout",1);
    if status
        try
            call(obj.rosCli_DeployUAV,req,"Timeout",1);
        catch
            status = false;
        end
    end


end


function status = RemoveUAV(obj,id)

    i = obj.getUAVindex(id);

    if i == -1
        return
    end

    UAV = obj.UAVs(i);
    obj.UAVs = [ obj.UAVs(1:i-1) obj.UAVs(i+1:end) ];

    req = ros2message(obj.rosCli_RemoveUAV);
    req.name  = UAV.id;  

    status = waitForServer(obj.rosCli_RemoveUAV,"Timeout",1);
    if status
        try
            call(obj.rosCli_RemoveUAV,req,"Timeout",1);
        catch
            status = false;
        end
    end
    

end


function RemoteCommand(obj,UAVid,on,velX,velY,velZ,rotZ,duration)

    i = obj.getUAVindex(UAVid);

    if i == -1
        return
    end

    UAV = obj.UAVs(i);
    if UAV.model ~= UAVmodels.MiniDroneCommanded
        return
    end

    msg = ros2message(UAV.rosPub_RemoteCommand);
    msg.uav_id        = UAV.id;
    msg.on            = on;
    msg.vel.linear.x  = velX;
    msg.vel.linear.y  = velY;
    msg.vel.linear.z  = velZ;
    msg.vel.angular.z = rotZ;
    msg.duration.sec  = int32(duration);
    send(UAV.rosPub_RemoteCommand,msg);
        
end


function SendFlightPlan(obj,UAVid,fp)
    % fp es una lista de filas con 7 componentes

    i = obj.getUAVindex(UAVid);

    if i == -1
        return
    end

    UAV = obj.UAVs(i);
    if UAV.model ~= UAVmodels.MiniDroneFP1
        return
    end

    msg = ros2message(UAV.rosPub);
    msg.plan_id       = uint16(1);  %de momento
    msg.uav_id        = UAV.id;
    msg.operator_id   = char(obj.name);

    for i = 1:length(fp(:,7))
        msg.route(i).pos.x = fp(i,1);
        msg.route(i).pos.y = fp(i,2);
        msg.route(i).pos.z = fp(i,3);
        msg.route(i).vel.x = fp(i,4);
        msg.route(i).vel.y = fp(i,5);
        msg.route(i).vel.z = fp(i,6);
        msg.route(i).time.sec = int32(fp(i,7));
        msg.route(i).time.nanosec = uint32(0);
    end
    send(UAV.rosPub,msg);
        
end


function index = getUAVindex(obj,id)
    index = -1;
    l = length(obj.UAVs);
    if l==0
        return
    end
    for i = 1:l
        if obj.UAVs(i).id == id
            index = i;
            return
        end
    end
end



end % methods 
end % classdef

