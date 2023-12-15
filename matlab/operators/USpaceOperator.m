% Operator classes represent operators in the context of U-space.
% Each operator has a drone garage where it stores its UAVs.

classdef USpaceOperator < handle      

properties
    name         string            % Operator name
    % id           uint16            % Unique ID (provided by the U-space registry service)
    % drone_garage UAVProperties     % Array of UAV objects references

    % ROS2 interface
    rosNode                        % ROS2 Node 
    rosCli_Test                    % ROS2 service client 
    rosCli_DeployModel             % ROS2 Service client to deploy models into the air space
    % ROScli_reg_operator            % Service client to register itself as operators
    % ROScli_reg_FP                  % Service client to register a new FP

end

methods


%Class constructor
function obj = DC_Operator(name)

    obj.name = name;
    
    % ROS2 node
    obj.rosNode = ros2node(obj.name);


    % ROS2 service clients
    obj.rosCli_Test = ros2svcclient(obj.rosNode, ...
        '/World/Test','navsim_msgs/Test');

    obj.rosCli_DeployModel = ros2svcclient(obj.rosNode, ...
        '/World/DeployModel','navsim_msgs/DeployModel', ...
        'History','keepall');




    % obj.ROScli_reg_operator = ros.ServiceClient(obj.ROSnode,"/utm/services/registry/register_operator");

    % ROS service client to register UAVs
    % obj.ROScli_reg_UAV = ros.ServiceClient(obj.ROSnode,"/utm/services/registry/register_UAV");

    % ROS service client to register flight plans
    % obj.ROScli_reg_FP = ros.ServiceClient(obj.ROSnode,"/utm/services/registry/register_FP");
    
    % %Sign up in the registry
    % if isServerAvailable(obj.ROScli_reg_operator)
    %     req = obj.ROScli_reg_operator.rosmessage;
    %     req.OperatorInfo.Name = obj.name;
    %     [connectionStatus,connectionStatustext] = waitForServer(client);
    %     if connectionStatus
    %         res = call(obj.ROScli_reg_operator, req, "Timeout", 3);
    %     else
    %         error("Error connecting to ROS service");
    %     end
    % else
    %     error("ROS server is not available");
    % end




    %Get operator ID
    % obj.id = res.OperatorInfo.Id;
end


function AirSpace_Test(obj)
    req = ros2message(obj.rosCli_Test);
    req.a = int16(2);
    req.b = int16(3);
        
    status = waitForServer(obj.rosCli_Test,"Timeout",3);
    if ~status
        error("Es servicio ROS2 no está disponible")
    end
    res = call(obj.rosCli_Test,req,"Timeout",3);
        
    if res.sum == 5
        disp("El servicio se ha llamado correctamente.")
    end
end


function status =  AirSpace_DeployUAV(obj,type,name,pos,rot)

    path = '../../ws/src/navsim_pkg/models/';

    switch type
        case 'drone'
            model = '../../ws/src/navsim_pkg/models/DCmodels/drone/model.sdf';
        case 'base'
            req.model_sdf = fileread('../../ws/src/navsim_pkg/models/DCmodels/base_drone/model.sdf');
        otherwise
            status = false;
            return
    end

    file = strcat(path,model,'/model.sdf');


    req = ros2message(obj.rosCli_DeployModel);
    req.model_sdf = fileread(file);
    req.name  = name;  %'deployedModel'
    req.pos.x = pos(1);
    req.pos.y = pos(2);
    req.pos.z = pos(3);
    req.rot.x = rot(1);
    req.rot.y = rot(2);
    req.rot.z = rot(3);

    status = waitForServer(obj.rosCli_DeployModel,"Timeout",1);
    if status
        try
            call(obj.rosCli_DeployModel,req,"Timeout",1);
        catch
            status = false;
        end
    end
        
    % if res.status == 1
    %      disp("El servicio se ha llamado correctamente.")
    % end
end




%Register a new drone to the operator adding it to the drone garage
% function UAVprop = regNewDrone(obj,model,init_pos)
% 
%     %Create UAVProperties
%     UAVprop = UAVProperties(obj.id,model,init_pos);
% 
%     if isServerAvailable(obj.ROScli_reg_UAV)
%         %Create request and fullfillment
%         req = obj.ROScli_reg_UAV.rosmessage;
%         req.Uav.OperatorId = obj.id;
%         req.Uav.Model = model;
%         req.InitPos = init_pos;
%         %Call service
%         res = call(obj.ROScli_reg_UAV, req, "Timeout", 3);
%     end
%     %Store properties
%     UAVprop.UAV_reg_message = res.Uav;
%     UAVprop.drone_id = res.Uav.Id;
%     UAVprop.pubsubToFlightPlan();
%     %Save in the garage
%     obj.drone_garage(end+1) = UAVprop;
% end


% function obj = regNewFP(obj,fp)
%     if isServerAvailable(obj.ROScli_reg_FP)
%         %Create request
%         reg_fp_req = obj.ROScli_reg_FP.rosmessage;
%         reg_fp_req.Fp = fp.parseToROSMessage();
%         %Call
%         reg_fp_res = call(obj.ROScli_reg_FP,reg_fp_req, "Timeout", 3);
%         %Save info
%         if (reg_fp_res.Status)
%             fp.flightplan_id = reg_fp_res.Fp.FlightPlanId;
%         else
%             fprintf("Flight Plan %d NOT accepted", reg_fp_res.Fp.flightPlanId);
%         end
%     end
% end


%Send a flight plan to a UAV
% function sendFlightPlan(obj, fp)
%     if isempty(fp.flightplan_id)
%         fprintf("Trying to send a FP to UAV without flightplan_id");
%         return;
%     end
%     %Get UAV and send FP
%     uav = fp.uav;
%     send(uav.ros_fp_pub, fp.parseToROSMessage());
%     fp.sent = 1;
% end
    

end % methods 
end % classdef

